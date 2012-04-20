/*
 * admCtrlWpa.c
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

/** \file admCtrl.c
 *  \brief Admission control API implimentation
 *
 *  \see admCtrl.h
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Admission Control	    		                                *
 *   PURPOSE: Admission Control Module API                              	*
 *                                                                          *
 ****************************************************************************/

#define __FILE_ID__  FILE_ID_19
#include "osApi.h"
#include "paramOut.h"
#include "mlmeApi.h"
#include "802_11Defs.h"
#include "DataCtrl_Api.h"
#include "report.h"
#include "rsn.h"
#include "admCtrl.h"
#include "admCtrlWpa.h"
#include "admCtrlWpa2.h"
#ifdef XCC_MODULE_INCLUDED
#include "admCtrlXCC.h"
#include "XCCMngr.h"
#endif
#include "siteMgrApi.h"
#include "TWDriver.h"

/* Constants */
#define MAX_NETWORK_MODE 2
#define MAX_WPA_CIPHER_SUITE 7



/* Enumerations */

/* Typedefs */

/* Structures */

/* External data definitions */

/* Local functions definitions */

/* Global variables */

static TI_UINT8 wpaIeOuiIe[3] = { 0x00, 0x50, 0xf2};

static TI_BOOL broadcastCipherSuiteValidity[MAX_NETWORK_MODE][MAX_WPA_CIPHER_SUITE]=
{
    /* RSN_IBSS */  {
/* NONE		  */    TI_FALSE,
/* WEP40	  */    TI_FALSE,
/* TKIP		  */    TI_TRUE,
/* AES_WRAP	  */    TI_TRUE,
/* AES_CCMP	  */    TI_TRUE,
/* WEP104     */    TI_FALSE,
/* CKIP       */    TI_FALSE},

    /* RSN_INFRASTRUCTURE */  {
/* NONE		  */    TI_FALSE,
/* WEP		  */    TI_TRUE,
/* TKIP		  */    TI_TRUE,
/* AES_WRAP	  */    TI_TRUE,
/* AES_CCMP	  */    TI_TRUE,
/* WEP104     */    TI_TRUE,
/* CKIP       */    TI_TRUE}
};

/** WPA admission table. Used to verify admission parameters to an AP */
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
} admCtrlWpa_validity_t;

static admCtrlWpa_validity_t    admCtrlWpa_validityTable[MAX_WPA_CIPHER_SUITE][MAX_WPA_CIPHER_SUITE][MAX_WPA_CIPHER_SUITE] =
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
            /* STA WEP */   { TI_OK,  TWD_CIPHER_WEP,  TWD_CIPHER_WEP ,1},
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA WRAP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_WEP,  TWD_CIPHER_WEP ,1},
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
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
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
            /* STA TKIP */  { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA AES */   { TI_NOK, TWD_CIPHER_NONE, TWD_CIPHER_NONE ,0},
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_AES_CCMP, TWD_CIPHER_AES_CCMP ,7},
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
            /* STA CCMP */  { TI_OK,  TWD_CIPHER_WEP104, TWD_CIPHER_WEP104 ,1},
            /* STA WEP104 */{ TI_OK,  TWD_CIPHER_WEP104, TWD_CIPHER_WEP104 ,1}}}


};

/* Function prototypes */
TI_STATUS admCtrlWpa_parseIe(admCtrl_t *pAdmCtrl, TI_UINT8 *pWpaIe, wpaIeData_t *pWpaData);
TI_UINT16 admCtrlWpa_buildCapabilities(TI_UINT16 replayCnt);
TI_UINT32  admCtrlWpa_parseSuiteVal(admCtrl_t *pAdmCtrl, TI_UINT8* suiteVal,wpaIeData_t *pWpaData,TI_UINT32 maxVal);
TI_STATUS admCtrlWpa_checkCipherSuiteValidity (ECipherSuite unicastSuite, ECipherSuite broadcastSuite, ECipherSuite encryptionStatus);
static TI_STATUS admCtrlWpa_get802_1x_AkmExists (admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists);


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
TI_STATUS admCtrlWpa_config(admCtrl_t *pAdmCtrl)
{
    TI_STATUS           status;
    TRsnPaeConfig       paeConfig;

    /* check and set admission control default parameters */
    pAdmCtrl->authSuite =   RSN_AUTH_OPEN;
    if (pAdmCtrl->unicastSuite == TWD_CIPHER_NONE)
    {
        pAdmCtrl->unicastSuite = TWD_CIPHER_TKIP;
    }
    if (pAdmCtrl->broadcastSuite == TWD_CIPHER_NONE)
    {
        pAdmCtrl->broadcastSuite = TWD_CIPHER_TKIP;
    }

    /* set callback functions (API) */
    pAdmCtrl->getInfoElement = admCtrlWpa_getInfoElement;
    pAdmCtrl->setSite = admCtrlWpa_setSite;
    pAdmCtrl->evalSite = admCtrlWpa_evalSite;

    pAdmCtrl->getPmkidList      = admCtrl_nullGetPMKIDlist;
    pAdmCtrl->setPmkidList      = admCtrl_nullSetPMKIDlist;
    pAdmCtrl->resetPmkidList    = admCtrl_resetPMKIDlist;
    pAdmCtrl->getPreAuthStatus = admCtrl_nullGetPreAuthStatus;
	pAdmCtrl->startPreAuth	= admCtrl_nullStartPreAuth;
    pAdmCtrl->get802_1x_AkmExists = admCtrlWpa_get802_1x_AkmExists;

    /* set cipher suite */                                  
    switch (pAdmCtrl->externalAuthMode)
    {
    case RSN_EXT_AUTH_MODE_WPA:
    case RSN_EXT_AUTH_MODE_WPAPSK:
        /* The cipher suite should be set by the External source via 
        the Encryption field*/
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




TI_STATUS admCtrlWpa_dynamicConfig(admCtrl_t *pAdmCtrl,wpaIeData_t *pWpaData)
{
    TI_STATUS           status;
    TRsnPaeConfig       paeConfig;


    /* set callback functions (API) */
    pAdmCtrl->getInfoElement = admCtrlWpa_getInfoElement;

    switch (pAdmCtrl->externalAuthMode)
    {
    case RSN_EXT_AUTH_MODE_WPA:
    case RSN_EXT_AUTH_MODE_WPAPSK:
        /* The cipher suite should be set by the External source via 
        the Encryption field*/
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_802_1X;
        break;
    case RSN_EXT_AUTH_MODE_WPANONE:
        pAdmCtrl->keyMngSuite = RSN_KEY_MNG_NONE;
        /* Not supported */
    default:
        return TI_NOK;
    }


    paeConfig.authProtocol = pAdmCtrl->externalAuthMode;
    paeConfig.unicastSuite = pWpaData->unicastSuite[0];
    paeConfig.broadcastSuite = pWpaData->broadcastSuite;
    paeConfig.keyExchangeProtocol = pAdmCtrl->keyMngSuite;
	/* set default PAE configuration */
    status = pAdmCtrl->pRsn->setPaeConfig(pAdmCtrl->pRsn, &paeConfig);

    return status;
}

/**
*
* admCtrlWpa_getInfoElement - Get the current information element.
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

TI_STATUS admCtrlWpa_getInfoElement(admCtrl_t *pAdmCtrl, TI_UINT8 *pIe, TI_UINT32 *pLength)
{
    wpaIePacket_t   localWpaPkt;
    wpaIePacket_t   *pWpaIePacket;
    TI_UINT8        length;
    TI_UINT16       tempInt;
    TIWLN_SIMPLE_CONFIG_MODE wscMode;

    /* Get Simple-Config state */
    siteMgr_getParamWSC(pAdmCtrl->pRsn->hSiteMgr, &wscMode); /* SITE_MGR_SIMPLE_CONFIG_MODE */

    if (pIe==NULL)
    {
        *pLength = 0;
        return TI_NOK;
    }

    if ((wscMode != TIWLN_SIMPLE_CONFIG_OFF) && 
        (pAdmCtrl->broadcastSuite == TWD_CIPHER_NONE) && 
        (pAdmCtrl->unicastSuite == TWD_CIPHER_NONE))
    {
      *pLength = 0;
      return TI_NOK;
    }
    
    /* Check validity of WPA IE */
    if (!broadcastCipherSuiteValidity[pAdmCtrl->networkMode][pAdmCtrl->broadcastSuite])
    {   /* check Group suite validity */                                          
        *pLength = 0;
        return TI_NOK;
    }
   
    
    if (pAdmCtrl->unicastSuite == TWD_CIPHER_WEP)
    {   /* check pairwise suite validity */                                       
        *pLength = 0;
        return TI_NOK;
    }

    /* Build Wpa IE */
    pWpaIePacket = &localWpaPkt;
    os_memoryZero(pAdmCtrl->hOs, pWpaIePacket, sizeof(wpaIePacket_t));
    pWpaIePacket->elementid= WPA_IE_ID;
    os_memoryCopy(pAdmCtrl->hOs, (void *)pWpaIePacket->oui, wpaIeOuiIe, 3);
    pWpaIePacket->ouiType = WPA_OUI_DEF_TYPE;

	tempInt = WPA_OUI_MAX_VERSION;
	COPY_WLAN_WORD(&pWpaIePacket->version, &tempInt);

    length = sizeof(wpaIePacket_t)-2;

    /* check defaults */
    if (pAdmCtrl->replayCnt==1)
    {
        length -= 2; /* 2: capabilities + 4: keyMng suite, 2: keyMng count*/
#if 0 /* The following was removed since there are APs which do no accept
	the default WPA IE */
		if (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA)
		{
			length -= 6; /* 2: capabilities + 4: keyMng suite, 2: keyMng count*/
			if (pAdmCtrl->unicastSuite == TWD_CIPHER_TKIP)
			{
				length -= 6; /* 4: unicast suite, 2: unicast count */
				if (pAdmCtrl->broadcastSuite == TWD_CIPHER_TKIP)
				{
					length -= 4;  /* broadcast suite */
				}
			}
		}
#endif
	}

    pWpaIePacket->length = length;
    *pLength = length+2;

	if (length>=WPA_IE_MIN_DEFAULT_LENGTH)
    {   /* build Capabilities */
        pWpaIePacket->capabilities = ENDIAN_HANDLE_WORD(admCtrlWpa_buildCapabilities(pAdmCtrl->replayCnt));
	}

	if (length>=WPA_IE_MIN_KEY_MNG_SUITE_LENGTH(1))
	{
        /* build keyMng suite */

		tempInt = 0x0001;
		COPY_WLAN_WORD(&pWpaIePacket->authKeyMngSuiteCnt, &tempInt);

        os_memoryCopy(pAdmCtrl->hOs, (void *)pWpaIePacket->authKeyMngSuite, wpaIeOuiIe, 3);
        
        switch (pAdmCtrl->externalAuthMode)
        {
        case RSN_EXT_AUTH_MODE_OPEN:
        case RSN_EXT_AUTH_MODE_SHARED_KEY:
        case RSN_EXT_AUTH_MODE_AUTO_SWITCH:
            pWpaIePacket->authKeyMngSuite[3] = WPA_IE_KEY_MNG_NONE;
            break;
		case RSN_EXT_AUTH_MODE_WPA:
			{
#ifdef XCC_MODULE_INCLUDED
				TI_UINT8	akmSuite[DOT11_OUI_LEN];

				if (admCtrlXCC_getCckmAkm(pAdmCtrl, akmSuite))
				{
					os_memoryCopy(pAdmCtrl->hOs, (void*)pWpaIePacket->authKeyMngSuite, akmSuite, DOT11_OUI_LEN);
				}
				else
#endif
				{
					pWpaIePacket->authKeyMngSuite[3] = WPA_IE_KEY_MNG_801_1X;
				}
			}

            break;

        case RSN_EXT_AUTH_MODE_WPAPSK:
            pWpaIePacket->authKeyMngSuite[3] = WPA_IE_KEY_MNG_PSK_801_1X;
            break;
        default:
            pWpaIePacket->authKeyMngSuite[3] = WPA_IE_KEY_MNG_NONE;
            break;
        }
         
    }

    
    if (length>=WPA_IE_MIN_PAIRWISE_SUITE_LENGTH)
    {  
 
#ifdef XCC_MODULE_INCLUDED       
        if ((pAdmCtrl->pRsn->paeConfig.unicastSuite==TWD_CIPHER_CKIP) || 
            (pAdmCtrl->pRsn->paeConfig.broadcastSuite==TWD_CIPHER_CKIP))
        {
           admCtrlXCC_getWpaCipherInfo(pAdmCtrl,pWpaIePacket);
        }
        else
#endif
        {
        
            /* build pairwise suite */

			tempInt = 0x0001;
			COPY_WLAN_WORD(&pWpaIePacket->pairwiseSuiteCnt, &tempInt);

            os_memoryCopy(pAdmCtrl->hOs, (void *)pWpaIePacket->pairwiseSuite, wpaIeOuiIe, 3);
            pWpaIePacket->pairwiseSuite[3] = (TI_UINT8)pAdmCtrl->pRsn->paeConfig.unicastSuite;
       
            if (length>=WPA_IE_GROUP_SUITE_LENGTH)
            {   /* build group suite */
                os_memoryCopy(pAdmCtrl->hOs, (void *)pWpaIePacket->groupSuite, wpaIeOuiIe, 3);
                pWpaIePacket->groupSuite[3] = (TI_UINT8)pAdmCtrl->pRsn->paeConfig.broadcastSuite;
            }
        }
    }
    os_memoryCopy(pAdmCtrl->hOs, (TI_UINT8*)pIe, (TI_UINT8*)pWpaIePacket, sizeof(wpaIePacket_t));
    return TI_OK;

}
/**
*
* admCtrlWpa_setSite  - Set current primary site parameters for registration.
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
TI_STATUS admCtrlWpa_setSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TI_UINT8 *pAssocIe, TI_UINT8 *pAssocIeLen)
{
    TI_STATUS           status;
    paramInfo_t         *pParam;
    TTwdParamInfo       tTwdParam;
    wpaIeData_t         wpaData;
    ECipherSuite        encryptionStatus;
    admCtrlWpa_validity_t *pAdmCtrlWpa_validity=NULL;
    TI_UINT8            *pWpaIe;
    TI_UINT8            index;

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
        goto adm_ctrl_wpa_end;
    }

#ifdef XCC_MODULE_INCLUDED
	/* Check if Aironet IE exists */
	admCtrlXCC_setExtendedParams(pAdmCtrl, pRsnData);
#endif /*XCC_MODULE_INCLUDED*/

    /* Check if any-WPA mode is supported and WPA2 info elem is presented */
    /* If yes - perform WPA2 set site  procedure                          */
    if(pAdmCtrl->WPAMixedModeEnable && pAdmCtrl->WPAPromoteFlags)
    {
        if((admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpaIe, RSN_IE_ID)== TI_OK) &&
           (pWpaIe != NULL))
        {
           status = admCtrlWpa2_setSite(pAdmCtrl, pRsnData,  pAssocIe, pAssocIeLen);
           if(status == TI_OK)
               goto adm_ctrl_wpa_end;
        }
    }
    
	status = admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpaIe, WPA_IE_ID);
	if (status != TI_OK)                                                         
	{                                                                                    
        goto adm_ctrl_wpa_end;
	}
    status = admCtrlWpa_parseIe(pAdmCtrl, pWpaIe, &wpaData);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa_end;
    }
    if ((wpaData.unicastSuite[0]>=MAX_WPA_CIPHER_SUITE) ||
        (wpaData.broadcastSuite>=MAX_WPA_CIPHER_SUITE) ||
        (pAdmCtrl->unicastSuite>=MAX_WPA_CIPHER_SUITE))
    {
        status = TI_NOK;
        goto adm_ctrl_wpa_end;
    }

    pAdmCtrl->encrInSw = wpaData.XCCKp;
    pAdmCtrl->micInSw = wpaData.XCCMic; 

    /*Because ckip is a proprietary encryption for Cisco then a different validity check is needed */
    if(wpaData.broadcastSuite == TWD_CIPHER_CKIP || wpaData.unicastSuite[0] ==  TWD_CIPHER_CKIP)
    {
        pAdmCtrl->getCipherSuite(pAdmCtrl, &encryptionStatus);
	/*Funk supplicant can support CCKM only if it configures the driver to TKIP encryption. */
        if (encryptionStatus != TWD_CIPHER_TKIP) {
            status = TI_NOK;
            goto adm_ctrl_wpa_end;
        }
        if (pAdmCtrl->encrInSw)
            pAdmCtrl->XCCSupport = TI_TRUE;
    }
    else
    {
        /* Check validity of Group suite */
        if (!broadcastCipherSuiteValidity[pAdmCtrl->networkMode][wpaData.broadcastSuite])
        {   /* check Group suite validity */                                          
            status = TI_NOK;
            goto adm_ctrl_wpa_end;
        }

        pAdmCtrl->getCipherSuite(pAdmCtrl, &encryptionStatus);
        for (index=0; index<wpaData.unicastSuiteCnt; index++)
        {
            pAdmCtrlWpa_validity = &admCtrlWpa_validityTable[wpaData.unicastSuite[index]][wpaData.broadcastSuite][encryptionStatus];
            if (pAdmCtrlWpa_validity->status ==TI_OK)
            {
                break;
            }
        }

        if (pAdmCtrlWpa_validity->status != TI_OK)
        {
            status = pAdmCtrlWpa_validity->status;
            goto adm_ctrl_wpa_end;
        }
   
        /* set cipher suites */
        wpaData.unicastSuite[0] = pAdmCtrlWpa_validity->unicast ;/*wpaData.unicastSuite[0];*/
        wpaData.broadcastSuite = pAdmCtrlWpa_validity->broadcast; /*wpaData.broadcastSuite;*/
    }
    /* set external auth mode according to the key Mng Suite */
    switch (wpaData.KeyMngSuite[0])
    {
    case WPA_IE_KEY_MNG_NONE:
        pAdmCtrl->externalAuthMode = RSN_EXT_AUTH_MODE_OPEN;
        break;
	case WPA_IE_KEY_MNG_801_1X:
#ifdef XCC_MODULE_INCLUDED
	case WPA_IE_KEY_MNG_CCKM:
#endif
        pAdmCtrl->externalAuthMode = RSN_EXT_AUTH_MODE_WPA;
        break;
    case WPA_IE_KEY_MNG_PSK_801_1X:
#if 0 /* code will remain here until the WSC spec will be closed*/
         if ((wpaData.KeyMngSuiteCnt > 1) && (wpaData.KeyMngSuite[1] == WPA_IE_KEY_MNG_801_1X))
        {
           /*WLAN_OS_REPORT (("Overriding for simple-config - setting external auth to MODE WPA\n"));*/
           /*pAdmCtrl->externalAuthMode = RSN_EXT_AUTH_MODE_WPA;*/
        }
         else
         {
            /*pAdmCtrl->externalAuthMode = RSN_EXT_AUTH_MODE_WPAPSK;*/
         }
#endif
        break;
    default:
        pAdmCtrl->externalAuthMode = RSN_EXT_AUTH_MODE_OPEN;
        break;
    }
      

#ifdef XCC_MODULE_INCLUDED
	pParam->paramType = XCC_CCKM_EXISTS;
	pParam->content.XCCCckmExists = (wpaData.KeyMngSuite[0]==WPA_IE_KEY_MNG_CCKM) ? TI_TRUE : TI_FALSE;
	XCCMngr_setParam(pAdmCtrl->hXCCMngr, pParam);
#endif
    /* set replay counter */
    pAdmCtrl->replayCnt = wpaData.replayCounters;

    *pAssocIeLen = pRsnData->ieLen;
    if (pAssocIe != NULL)
    {
        os_memoryCopy(pAdmCtrl->hOs, pAssocIe, &wpaData, sizeof(wpaIeData_t));
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
        goto adm_ctrl_wpa_end;
    }

    pParam->paramType = RX_DATA_EAPOL_DESTINATION_PARAM;
    pParam->content.rxDataEapolDestination = OS_ABS_LAYER;
    status = rxData_setParam(pAdmCtrl->hRx, pParam);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa_end;
    }

	/* Configure privacy status in HAL so that HW is prepared to recieve keys */
	tTwdParam.paramType = TWD_RSN_SECURITY_MODE_PARAM_ID;   
	tTwdParam.content.rsnEncryptionStatus = (ECipherSuite)wpaData.unicastSuite[0];
	status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
	if (status != TI_OK)
	{
        goto adm_ctrl_wpa_end;		
	}

#ifdef XCC_MODULE_INCLUDED
	    
	/* set MIC and KP in HAL  */
    tTwdParam.paramType = TWD_RSN_XCC_SW_ENC_ENABLE_PARAM_ID; 
    tTwdParam.content.rsnXCCSwEncFlag = wpaData.XCCKp;
    status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa_end;
    }
    tTwdParam.paramType = TWD_RSN_XCC_MIC_FIELD_ENABLE_PARAM_ID; 
    tTwdParam.content.rsnXCCMicFieldFlag = wpaData.XCCMic;
    status = TWD_SetParam(pAdmCtrl->pRsn->hTWD, &tTwdParam);
    
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa_end;        
    }
#endif /*XCC_MODULE_INCLUDED*/

    /* re-config PAE */
    status = admCtrlWpa_dynamicConfig(pAdmCtrl,&wpaData);
    if (status != TI_OK)
    {
        goto adm_ctrl_wpa_end;
    }
adm_ctrl_wpa_end:
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
TI_STATUS admCtrlWpa_evalSite(admCtrl_t *pAdmCtrl, TRsnData *pRsnData, TRsnSiteParams *pRsnSiteParams, TI_UINT32 *pEvaluation)
{
    TI_STATUS               status;
    wpaIeData_t             wpaData;
    admCtrlWpa_validity_t   admCtrlWpa_validity;
    ECipherSuite            encryptionStatus;
    TIWLN_SIMPLE_CONFIG_MODE wscMode;
    TI_UINT8                *pWpaIe;
    TI_UINT8                index;

	/* Get Simple-Config state */
    status = siteMgr_getParamWSC(pAdmCtrl->pRsn->hSiteMgr, &wscMode); /* SITE_MGR_SIMPLE_CONFIG_MODE */

	*pEvaluation = 0;

    if (pRsnData==NULL)
    {
        return TI_NOK;
    }
    if ((pRsnData->pIe==NULL) && (wscMode == TIWLN_SIMPLE_CONFIG_OFF))
    {
        return TI_NOK;
    }
    
    if (pRsnSiteParams->bssType != BSS_INFRASTRUCTURE)
    {
        return TI_NOK;
    }
	
    /* Set initial values for admCtrlWpa_validity as none*/
    admCtrlWpa_validity = admCtrlWpa_validityTable[TWD_CIPHER_NONE][TWD_CIPHER_NONE][TWD_CIPHER_NONE];

   	/* Check if WPA-any mode is supported and WPA2 info elem is presented */
    /* If yes - perform WPA2 site evaluation                              */
    if(pAdmCtrl->WPAMixedModeEnable && pAdmCtrl->WPAPromoteFlags)
    {
    	if((admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpaIe, RSN_IE_ID)== TI_OK)  &&
           (pWpaIe != NULL))
        {
            status = admCtrlWpa2_evalSite(pAdmCtrl, pRsnData, pRsnSiteParams, pEvaluation);
            if(status == TI_OK)
                return status;
        }
    }

	status = admCtrl_parseIe(pAdmCtrl, pRsnData, &pWpaIe, WPA_IE_ID);
	if ((status != TI_OK) && (wscMode == TIWLN_SIMPLE_CONFIG_OFF))                                                        
	{                                                                                    
		return status;                                                        
	}
    /* If found WPA Information Element */
    if (pWpaIe != NULL)
    {
    status = admCtrlWpa_parseIe(pAdmCtrl, pWpaIe, &wpaData);
    if (status != TI_OK)
    {
        return status;
    }

	/* check keyMngSuite validity */
    switch (wpaData.KeyMngSuite[0])
    {
    case WPA_IE_KEY_MNG_NONE:
        TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa_evalSite: KeyMngSuite[0]=WPA_IE_KEY_MNG_NONE\n");
        status = (pAdmCtrl->externalAuthMode <= RSN_EXT_AUTH_MODE_AUTO_SWITCH) ? TI_OK : TI_NOK;
		break;
    case WPA_IE_KEY_MNG_801_1X:
#ifdef XCC_MODULE_INCLUDED
	case WPA_IE_KEY_MNG_CCKM:
		/* CCKM is allowed only in 802.1x auth */
#endif
       TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa_evalSite: KeyMngSuite[0]=WPA_IE_KEY_MNG_801_1X\n");
        status = (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA) ? TI_OK : TI_NOK;
		break;
    case WPA_IE_KEY_MNG_PSK_801_1X:
       TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa_evalSite: KeyMngSuite[0]=WPA_IE_KEY_MNG_PSK_801_1X\n");
        status = ((pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPAPSK) ||
					(wscMode && (pAdmCtrl->externalAuthMode == RSN_EXT_AUTH_MODE_WPA))) ? TI_OK : TI_NOK;        		
        break;
    default:
        status = TI_NOK;
        break;
    }

    TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "admCtrlWpa_evalSite: pAdmCtrl->externalAuthMode = %d, Status = %d\n",pAdmCtrl->externalAuthMode,status);

    if (status != TI_OK)
    {
        return status;
    }

	/*Because ckip is a proprietary encryption for Cisco then a different validity check is needed */
    if(wpaData.broadcastSuite == TWD_CIPHER_CKIP || wpaData.unicastSuite[0] ==  TWD_CIPHER_CKIP)
    {
        pAdmCtrl->getCipherSuite(pAdmCtrl, &encryptionStatus);
        if (encryptionStatus != TWD_CIPHER_TKIP) 
            return TI_NOK;
    }
    else
    {
		/* Check cipher suite validity */
        pAdmCtrl->getCipherSuite(pAdmCtrl, &encryptionStatus);
        for (index=0; index<wpaData.unicastSuiteCnt; index++)
        {
			admCtrlWpa_validity = admCtrlWpa_validityTable[wpaData.unicastSuite[index]][wpaData.broadcastSuite][encryptionStatus];
            if (admCtrlWpa_validity.status ==TI_OK)
            {
                break;
            }
        }

        if (admCtrlWpa_validity.status!=TI_OK)
        {
            return admCtrlWpa_validity.status;
        }

		wpaData.broadcastSuite  = admCtrlWpa_validity.broadcast;
        wpaData.unicastSuite[0] = admCtrlWpa_validity.unicast;
        *pEvaluation = admCtrlWpa_validity.evaluation;
    }

	/* Check privacy bit if not in mixed mode */
    if (!pAdmCtrl->mixedMode)
    {   /* There's no mixed mode, so make sure that the privacy Bit matches the privacy mode*/
        if (((pRsnData->privacy) && (wpaData.unicastSuite[0]==TWD_CIPHER_NONE)) ||
            ((!pRsnData->privacy) && (wpaData.unicastSuite[0]>TWD_CIPHER_NONE)))
        {
            *pEvaluation = 0;
        }
    }

    }
    else
    {
       TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "didn't find WPA IE\n");
       if (wscMode == TIWLN_SIMPLE_CONFIG_OFF)
          return TI_NOK;
       TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "metric is 1\n");
       *pEvaluation = 1;
        pAdmCtrl->broadcastSuite = TWD_CIPHER_NONE;
        pAdmCtrl->unicastSuite = TWD_CIPHER_NONE;
    }

	/* always return TI_OK */
    return TI_OK;
}


/**
*
* admCtrlWpa_parseIe  - Parse an WPA information element.
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
*  I   - pWpaIe - pointer to WPA IE buffer  \n
*  O   - pWpaData - capabilities structure
*  
*  
* \b RETURNS:
*
* TI_OK on success, TI_NOK on failure. 
*
* \sa 
*/
TI_STATUS admCtrlWpa_parseIe(admCtrl_t *pAdmCtrl, TI_UINT8 *pWpaIe, wpaIeData_t *pWpaData)
{

    wpaIePacket_t   *wpaIePacket = (wpaIePacket_t*)pWpaIe;
    TI_UINT8           *curWpaIe;
    TI_UINT8           curLength = WPA_IE_MIN_LENGTH;

    TRACE0(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: DEBUG: admCtrlWpa_parseIe\n\n");

    if ((pWpaData == NULL) || (pWpaIe == NULL))
    {
        return TI_NOK;
    }

    if ((wpaIePacket->length < WPA_IE_MIN_LENGTH) ||
        (wpaIePacket->elementid != WPA_IE_ID) ||
        (wpaIePacket->ouiType > WPA_OUI_MAX_TYPE) || (ENDIAN_HANDLE_WORD(wpaIePacket->version) > WPA_OUI_MAX_VERSION) ||               
        (os_memoryCompare(pAdmCtrl->hOs, (TI_UINT8*)wpaIePacket->oui, wpaIeOuiIe, 3)))
    {
        TRACE7(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_ParseIe Error: length=0x%x, elementid=0x%x, ouiType=0x%x, version=0x%x, oui=0x%x, 0x%x, 0x%x\n", wpaIePacket->length,wpaIePacket->elementid, wpaIePacket->ouiType, wpaIePacket->version, wpaIePacket->oui[0], wpaIePacket->oui[1],wpaIePacket->oui[2]);

        return TI_NOK; 
    }
    /* Set default values */
    pWpaData->broadcastSuite = TWD_CIPHER_TKIP;
    pWpaData->unicastSuiteCnt = 1;
    pWpaData->unicastSuite[0] = TWD_CIPHER_TKIP;
    pWpaData->KeyMngSuiteCnt = 1;
    pWpaData->KeyMngSuite[0] = (ERsnKeyMngSuite)WPA_IE_KEY_MNG_801_1X;
    pWpaData->bcastForUnicatst = 1;
    pWpaData->replayCounters = 1;

    pWpaData->XCCKp = TI_FALSE;
    pWpaData->XCCMic = TI_FALSE;


    /* Group Suite */
    if (wpaIePacket->length >= WPA_IE_GROUP_SUITE_LENGTH)
    {
        pWpaData->broadcastSuite = (ECipherSuite)admCtrlWpa_parseSuiteVal(pAdmCtrl, (TI_UINT8 *)wpaIePacket->groupSuite,pWpaData,TWD_CIPHER_WEP104);
        curLength = WPA_IE_GROUP_SUITE_LENGTH;
        TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: GroupSuite%x, broadcast %x \n", wpaIePacket->groupSuite[3], pWpaData->broadcastSuite);
    } else
    {
        return TI_OK;
    }
    /* Unicast Suite */ 
    if (wpaIePacket->length >= WPA_IE_MIN_PAIRWISE_SUITE_LENGTH)
    {
        TI_UINT16 pairWiseSuiteCnt = ENDIAN_HANDLE_WORD(wpaIePacket->pairwiseSuiteCnt);
        TI_BOOL   cipherSuite[MAX_WPA_UNICAST_SUITES]={TI_FALSE, TI_FALSE, TI_FALSE, TI_FALSE, TI_FALSE, TI_FALSE , TI_FALSE};
        TI_INT32  index, unicastSuiteIndex=0;

        curWpaIe = (TI_UINT8*)&(wpaIePacket->pairwiseSuite);
        for (index=0; (index<pairWiseSuiteCnt) && (wpaIePacket->length >= (WPA_IE_MIN_PAIRWISE_SUITE_LENGTH+(index+1)*4)); index++)
        {
            ECipherSuite   curCipherSuite;

            curCipherSuite = (ECipherSuite)admCtrlWpa_parseSuiteVal(pAdmCtrl, curWpaIe,pWpaData,TWD_CIPHER_WEP104);
            TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: pairwiseSuite %x , unicast %x \n", curWpaIe[3], curCipherSuite);

            if ((curCipherSuite!=TWD_CIPHER_UNKNOWN) && (curCipherSuite<MAX_WPA_UNICAST_SUITES))
            {
                cipherSuite[curCipherSuite] =  TI_TRUE;
            }
            curWpaIe +=4; 
        }
        for (index=MAX_WPA_UNICAST_SUITES-1; index>=0; index--)
        {
            if (cipherSuite[index])
            {
                pWpaData->unicastSuite[unicastSuiteIndex] = (ECipherSuite)index;
                TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: unicast %x \n", pWpaData->unicastSuite[unicastSuiteIndex]);
                unicastSuiteIndex++;
            }
        }
        pWpaData->unicastSuiteCnt = unicastSuiteIndex;
        curLength = WPA_IE_MIN_KEY_MNG_SUITE_LENGTH(pairWiseSuiteCnt);
        
    } else
    {
        return TI_OK;
    }
    /* KeyMng Suite */
    if (wpaIePacket->length >= curLength)
    {
        TI_UINT16              keyMngSuiteCnt = ENDIAN_HANDLE_WORD(*curWpaIe);
        TI_UINT16              index;
        ERsnKeyMngSuite   maxKeyMngSuite = (ERsnKeyMngSuite)WPA_IE_KEY_MNG_NONE;

        /* Include all AP key management supported suites in the wpaData structure */
        pWpaData->KeyMngSuiteCnt = keyMngSuiteCnt;

        curWpaIe +=2;
        pAdmCtrl->wpaAkmExists = TI_FALSE;
        for (index=0; (index<keyMngSuiteCnt) && (wpaIePacket->length >= (curLength+index*4)); index++)
        {
            ERsnKeyMngSuite curKeyMngSuite;

#ifdef XCC_MODULE_INCLUDED
            curKeyMngSuite = (ERsnKeyMngSuite)admCtrlXCC_parseCckmSuiteVal(pAdmCtrl, curWpaIe);
			if (curKeyMngSuite == WPA_IE_KEY_MNG_CCKM)
			{	/* CCKM is the maximum AKM */
				maxKeyMngSuite =  curKeyMngSuite;
			}
			else
#endif
			{
				curKeyMngSuite = (ERsnKeyMngSuite)admCtrlWpa_parseSuiteVal(pAdmCtrl, curWpaIe,pWpaData,WPA_IE_KEY_MNG_PSK_801_1X);
			}
            TRACE2(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: authKeyMng %x , keyMng %x \n", curWpaIe[3], curKeyMngSuite);

            if ((curKeyMngSuite>maxKeyMngSuite) && (curKeyMngSuite!=WPA_IE_KEY_MNG_NA)
				&& (curKeyMngSuite!=WPA_IE_KEY_MNG_CCKM))
            {
                maxKeyMngSuite =  curKeyMngSuite;
            }
            if (curKeyMngSuite==WPA_IE_KEY_MNG_801_1X)
            {   /* If 2 AKM exist, save also the second priority */
                pAdmCtrl->wpaAkmExists = TI_TRUE;
            }

            curWpaIe +=4; 

            /* Include all AP key management supported suites in the wpaData structure */
	    if ((index+1) < MAX_WPA_KEY_MNG_SUITES)
                pWpaData->KeyMngSuite[index+1] = curKeyMngSuite;

        }
        pWpaData->KeyMngSuite[0] = maxKeyMngSuite;
        curLength += (index-1)*4;
        TRACE1(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: keyMng %x \n", pWpaData->KeyMngSuite[0]);

    } else
    {
        return TI_OK;
    }
    /* Parse capabilities */
    if (wpaIePacket->length >= (curLength+2))
    {
        TI_UINT16 capabilities = ENDIAN_HANDLE_WORD(*((TI_UINT16 *)curWpaIe));

        pWpaData->bcastForUnicatst = (capabilities & WPA_GROUP_4_UNICAST_CAPABILITY_MASK) >> WPA_REPLAY_GROUP4UNI_CAPABILITY_SHIFT;
        pWpaData->replayCounters =   (capabilities & WPA_REPLAY_COUNTERS_CAPABILITY_MASK) >> WPA_REPLAY_COUNTERS_CAPABILITY_SHIFT;
        switch (pWpaData->replayCounters)
        {
        case 0: pWpaData->replayCounters=1;
            break;
        case 1: pWpaData->replayCounters=2;
            break;
        case 2: pWpaData->replayCounters=4;
            break;
        case 3: pWpaData->replayCounters=16;
            break;
        default: pWpaData->replayCounters=0;
            break;
        }
        TRACE3(pAdmCtrl->hReport, REPORT_SEVERITY_INFORMATION, "Wpa_IE: capabilities %x, bcastForUnicatst %x, replayCounters %x\n", capabilities, pWpaData->bcastForUnicatst, pWpaData->replayCounters);

    }


    return TI_OK;

}


TI_UINT16 admCtrlWpa_buildCapabilities(TI_UINT16 replayCnt)
{
    TI_UINT16 capabilities=0;
    /* Bit1: group key for unicast */
    capabilities = 0;
    capabilities = capabilities << WPA_REPLAY_GROUP4UNI_CAPABILITY_SHIFT;
    /* Bits 2&3: Replay counter */
    switch (replayCnt)
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

    capabilities |= replayCnt << WPA_REPLAY_COUNTERS_CAPABILITY_SHIFT;
    return 	capabilities;

}


TI_UINT32  admCtrlWpa_parseSuiteVal(admCtrl_t *pAdmCtrl, TI_UINT8* suiteVal, wpaIeData_t *pWpaData, TI_UINT32 maxVal)
{
    TI_UINT32  suite;

    if ((pAdmCtrl==NULL) || (suiteVal==NULL))
    {
        return TWD_CIPHER_UNKNOWN;
    }
    if (!os_memoryCompare(pAdmCtrl->hOs, suiteVal, wpaIeOuiIe, 3))
    {
        suite =  (ECipherSuite)((suiteVal[3]<=maxVal) ? suiteVal[3] : TWD_CIPHER_UNKNOWN); 
    } else
    {
#ifdef XCC_MODULE_INCLUDED
        suite = admCtrlXCC_WpaParseSuiteVal(pAdmCtrl,suiteVal,pWpaData);
#else
        suite = TWD_CIPHER_UNKNOWN;
#endif
    }
    return 	suite;
}


TI_STATUS admCtrlWpa_checkCipherSuiteValidity (ECipherSuite unicastSuite, ECipherSuite broadcastSuite, ECipherSuite encryptionStatus)
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

static TI_STATUS admCtrlWpa_get802_1x_AkmExists (admCtrl_t *pAdmCtrl, TI_BOOL *wpa_802_1x_AkmExists)
{
    *wpa_802_1x_AkmExists = pAdmCtrl->wpaAkmExists;
    return TI_OK;
}



