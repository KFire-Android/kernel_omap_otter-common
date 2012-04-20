/*
 * ticon.c
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */

/****************************************************************************
*
*   MODULE:  ticon.c
*   
*   PURPOSE: 
* 
*   DESCRIPTION:  
*   ============
*      
*
****************************************************************************/

/* includes */
/************/
#include "cu_osapi.h"
#include "oserr.h"

#include "TWDriver.h"
#include "STADExternalIf.h"

#include "console.h"
#include "cu_cmd.h"
#include "wpa_core.h"
#ifdef XCC_MODULE_INCLUDED
#include "cu_XCC.h"
#endif

/* defines */
/***********/
#ifdef DEBUG
#define CHK_NULL(p)    ((p)) ? (VOID) 0 : os_error_printf(CU_MSG_ERROR, (PS8)"\nfailed: '%s', file %s, line %d\n", #p, __FILE__, __LINE__);
#define CHK(p)        ((!p)) ? (VOID) 0 : os_error_printf(CU_MSG_ERROR, (PS8)"\nfailed: '%s', file %s, line %d\n", #p, __FILE__, __LINE__);
#else
#define CHK(p)        (p)
#define CHK_NULL(p)    (p)
#endif

#define TIWLAN_DRV_NAME "tiwlan0"
#ifdef ANDROID
#define SUPPL_IF_FILE "/data/misc/wifi/sockets/" TIWLAN_DRV_NAME
#else
#define SUPPL_IF_FILE "/var/run/" TIWLAN_DRV_NAME
#endif
extern int consoleRunScript( char *script_file, THandle hConsole);
    
/* local types */
/***************/
/* Module control block */
typedef struct TiCon_t
{
	THandle 	hConsole;	
    S8          drv_name[IF_NAME_SIZE];
} TiCon_t;

/* local variables */
/*******************/
static TiCon_t g_TiCon;

/* local fucntions */
/*******************/
static S32 TiCon_Init_Console_Menu(TiCon_t* pTiCon)
{
    THandle h, h1;
    THandle h2;



    /* -------------------------------------------- Driver -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Driver", (PS8)"Driver start/stop" ) );	
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Start", (PS8)"Start driver", (FuncToken_t) CuCmd_StartDriver, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"sTop", (PS8)"Stop driver",  (FuncToken_t) CuCmd_StopDriver,  NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"stAtus", (PS8)"Print status", (FuncToken_t) CuCmd_Show_Status, NULL );
	
    /* -------------------------------------------- Connection -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Connection", (PS8)"Connection management" ) );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Bssid_list", (PS8)"Bssid_list", (FuncToken_t) CuCmd_BssidList, NULL );
	{
		ConParm_t aaa[]  = { {(PS8)"ssid", 	CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
		                     {(PS8)"bssid", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
							 CON_LAST_PARM };
		
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Connect", (PS8)"Connect", (FuncToken_t) CuCmd_Connect, aaa );
	}
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Disassociate", (PS8)"disconnect", (FuncToken_t) CuCmd_Disassociate, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Status", (PS8)"Print connection status", (FuncToken_t) CuCmd_Show_Status, NULL );
#ifdef TI_DBG 
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Full_bssid_list", (PS8)"Full_bssid_list", (FuncToken_t) CuCmd_FullBssidList, NULL );
#endif
	
#ifdef CONFIG_WPS
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"wPs", (PS8)"WiFi Protected Setup" ) );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Pin", (PS8)"Acquire profile using PIN", (FuncToken_t) CuCmd_StartEnrolleePIN, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"pBc", (PS8)"Acquire profile using Push Button", (FuncToken_t) CuCmd_StartEnrolleePBC, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Stop", (PS8)"Stop WiFi Protected Setup", (FuncToken_t) CuCmd_StopEnrollee, NULL );
	{
		ConParm_t aaa[]  =
		{
			{(PS8)"PIN", CON_PARM_STRING, 0, 8, 0 },
				CON_LAST_PARM
		};	
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"set pIn", (PS8)"Set PIN code", (FuncToken_t) CuCmd_SetPin, aaa );
	}
#endif /* CONFIG_WPS */

    /* -------------------------------------------- Management -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Management", (PS8)"Station management" ) );
	{
		ConParm_t aaa[]  = { {(PS8)"connectMode", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"connect moDe", (PS8)"Set Connect Mode", (FuncToken_t) CuCmd_ModifyConnectMode, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"channel", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Channel", (PS8)"Set the channel", (FuncToken_t) CuCmd_ModifyChannel, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"tx rate", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Rate", (PS8)"Get TX data rate in Mbps (1,2,5.5,11...)", (FuncToken_t) CuCmd_GetTxRate, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"BSS_type", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Mode", (PS8)"BSS_type", (FuncToken_t) CuCmd_ModifyBssType, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"frag", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Frag", (PS8)"Set the fragmentation threshold <256..2346>", (FuncToken_t) CuCmd_ModifyFragTh, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"rts", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"rts", (PS8)"Set RTS threshold <0..2347>", (FuncToken_t) CuCmd_ModifyRtsTh, aaa);
	}
	{
		ConParm_t aaa[]  = { {(PS8)"preamble", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 1, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Preamble", (PS8)"Set preamble type 1=short 0=long", (FuncToken_t) CuCmd_ModifyPreamble, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"slot", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"sLot", (PS8)"Set short  slot", (FuncToken_t) CuCmd_ModifyShortSlot, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"radio on//off", CON_PARM_OPTIONAL, 0, 1, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"rAdio on/off", (PS8)"Turn radio on/off. 0=OFF, 1=ON", (FuncToken_t) CuCmd_RadioOnOff, aaa );
	}
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Info", (PS8)"Get Selected BSSID Info", (FuncToken_t) CuCmd_GetSelectedBssidInfo, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"siGnal", (PS8)"Get Current RSSI level", (FuncToken_t) CuCmd_GetRsiiLevel, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"snr ratiO", (PS8)"Get Current SNR radio", (FuncToken_t) CuCmd_GetSnrRatio, NULL );
	
	
	{
		ConParm_t aaa[]  = { {(PS8)"Tx power level", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"tX_power_table", (PS8)"show Tx power table", (FuncToken_t) CuCmd_ShowTxPowerTable, aaa );
		Console_AddToken(pTiCon->hConsole,h, (PS8)"tx_power_dBm_div10", (PS8)"Tx power level", (FuncToken_t) CuCmd_TxPowerDbm, aaa );
	}	
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"802_11d_h",  (PS8)"802_11D_H" ) );
	{
		ConParm_t aaa[]  = { {(PS8)"802_11_D", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		ConParm_t bbb[]  = {
			{(PS8)"min DFS channel", CON_PARM_RANGE, 36, 180, 40 },
			{(PS8)"max DFS channel", CON_PARM_RANGE, 36, 180, 140 },
			CON_LAST_PARM};
			
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"D_enableDisable", (PS8)"enableDisable_d", (FuncToken_t) CuCmd_ModifyState_802_11d, aaa );
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"H_enableDisable", (PS8)"enableDisable_h", (FuncToken_t) CuCmd_ModifyState_802_11h, aaa );
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"d_Country_2_4Ie", (PS8)"d_Country_2_4Ie", (FuncToken_t) CuCmd_D_Country_2_4Ie, aaa );
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"d_cOuntry_5Ie", (PS8)"d_Country_5Ie", (FuncToken_t) CuCmd_D_Country_5Ie, aaa );
			
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"dfS_range", (PS8)"DFS_range", (FuncToken_t) CuCmd_ModifyDfsRange, bbb );
			
	}
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"beacoN", (PS8)"Set Beacon Filter Desired State" ) );
	{
		ConParm_t beaconFilterDesiredState[]  = { {(PS8)"Set Beacon Desired State", CON_PARM_OPTIONAL, 0, 0, 0 },
			CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Set Beacon Filter Desired State", (PS8)"Set Beacon Filter Current State", (FuncToken_t) CuCmd_SetBeaconFilterDesiredState, beaconFilterDesiredState );
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Get Beacon Filter Current State", (PS8)"Get Beacon Filter Current State", (FuncToken_t) CuCmd_GetBeaconFilterDesiredState, NULL );
	}
	
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole, (THandle) h, (PS8)"adVanced", (PS8)"Advanced params" ) );
	{
		ConParm_t aaa[]  = { { (PS8)"rates", CON_PARM_OPTIONAL | CON_PARM_LINE, 0, 120, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole, h1, (PS8)"Supported rates",  (PS8)"rates", (FuncToken_t) CuCmd_ModifySupportedRates, aaa );
	}
    Console_AddToken(pTiCon->hConsole, h1, (PS8)"Health-check", (PS8)"Send health-check to device", (FuncToken_t) CuCmd_SendHealthCheck, NULL );
	CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole, (THandle) h1, (PS8)"rx data Filter", (PS8)"Rx Data Filter" ) );
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Enable",  (PS8)"Enable Rx Data Filtering", (FuncToken_t) CuCmd_EnableRxDataFilters, NULL );
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Disable", (PS8)"Enable Rx Data Filtering", (FuncToken_t) CuCmd_DisableRxDataFilters, NULL );
    {
        ConParm_t aaa[]  = 
        {
            {(PS8)"Offset", CON_PARM_RANGE, 0, 255, 0 },
            {(PS8)"Mask", CON_PARM_STRING, 0, 64, 0 },
            {(PS8)"Pattern", CON_PARM_STRING, 0, 128, 0 },
            CON_LAST_PARM
        };
        Console_AddToken(pTiCon->hConsole, h2, (PS8)"Add", (PS8)"Add Rx Data Filter", (FuncToken_t) CuCmd_AddRxDataFilter, aaa );
    }
    {
        ConParm_t aaa[]  = 
        {
            {(PS8)"Offset", CON_PARM_RANGE, 0, 255, 0 },
            {(PS8)"Mask", CON_PARM_STRING, 0, 64, 0 },
            {(PS8)"Pattern", CON_PARM_STRING, 0, 128, 0 },
            CON_LAST_PARM
        };
        Console_AddToken(pTiCon->hConsole, h2, (PS8)"Remove", (PS8)"Remove Rx Data Filter", (FuncToken_t) CuCmd_RemoveRxDataFilter, aaa );
    }
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Statistics", (PS8)"Print Rx Data Filtering Statistics", (FuncToken_t) CuCmd_GetRxDataFiltersStatistics, NULL );
    CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole, (THandle) h1, (PS8)"Keep alive", (PS8)"Keep Alive templates" ) );
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Enable", (PS8)"Set global keep-alive flag to enable", (FuncToken_t)CuCmd_EnableKeepAlive, NULL );
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Disable", (PS8)"Set global keep-alive flag to disable", (FuncToken_t)CuCmd_DisableKeepAlive, NULL );
    {
        ConParm_t aaa[]  = 
        {
            {(PS8)"Index", CON_PARM_RANGE, 0, 1, 0 },
            {(PS8)"Interval (msec)", CON_PARM_RANGE, 0, 1000000, 60000 },
            {(PS8)"Trigger type (0-idle 1-always)", CON_PARM_RANGE, 0, 1, 0 },
            {(PS8)"Pattern (hex data)", CON_PARM_STRING, 0, KEEP_ALIVE_TEMPLATE_MAX_LENGTH * 2, 0 },
            CON_LAST_PARM
        };
        Console_AddToken(pTiCon->hConsole, h2, (PS8)"Add", (PS8)"Add a new keep-alive template", (FuncToken_t)CuCmd_AddKeepAliveMessage, aaa );
    }
    {
        ConParm_t aaa[]  = 
        {
            {(PS8)"Index", CON_PARM_RANGE, 0, 1, 0 },
            CON_LAST_PARM
        };
        Console_AddToken(pTiCon->hConsole, h2, (PS8)"Remove", (PS8)"Remove a keep-alive template", (FuncToken_t)CuCmd_RemoveKeepAliveMessage, aaa );
    }
    Console_AddToken(pTiCon->hConsole, h2, (PS8)"Show", (PS8)"Show all configured keep-alive templates", (FuncToken_t)CuCmd_ShowKeepAlive, NULL );

	/* -------------------------------------------- Show -------------------------------------------- */	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Show", (PS8)"Show params" ) );
	Console_AddToken(pTiCon->hConsole, h, (PS8)"Statistics", (PS8)"Show statistics", (FuncToken_t) CuCmd_ShowStatistics, NULL );
	{
		ConParm_t aaa[]  = { {(PS8)"Clear stats on read", CON_PARM_OPTIONAL | CON_PARM_RANGE, 0, 1, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Txstatistics", (PS8)"Show tx statistics", (FuncToken_t) CuCmd_ShowTxStatistics, aaa );
	}
    Console_AddToken(pTiCon->hConsole,h, (PS8)"Advanced", (PS8)"Show advanced params", (FuncToken_t) CuCmd_ShowAdvancedParams, NULL );

    Console_AddToken(pTiCon->hConsole,h, (PS8)"Power consumption",  (PS8)"Show power consumption statistics", (FuncToken_t) Cucmd_ShowPowerConsumptionStats, NULL );
	
	/* -------------------------------------------- Privacy -------------------------------------------- */
	
	CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Privacy", (PS8)"Privacy configuration" ) );
	{
		ConParm_t aaa[]  = { {(PS8)"mode", CON_PARM_OPTIONAL, 0, 0, 0 },CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Authentication", (PS8)"Set authentication mode",
			(FuncToken_t)CuCmd_SetPrivacyAuth, aaa ); 
	}
#ifdef WPA_ENTERPRISE
	{
		ConParm_t aaa[]  = { {(PS8)"type", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Eap", (PS8)"Set EAP type", (FuncToken_t)CuCmd_SetPrivacyEap, aaa );
	}
#endif    
	{
		ConParm_t aaa[]  = { {(PS8)"type", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"encRyption", (PS8)"Set encryption type", (FuncToken_t)CuCmd_SetPrivacyEncryption, aaa);
	}	
#ifdef WPA_ENTERPRISE
	{
		ConParm_t aaa[]  = { {(PS8)"type", 0, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Keytype", (PS8)"Set key type", (FuncToken_t) CuCmd_SetPrivacyKeyType, aaa );
	}
	
	{
		ConParm_t aaa[]  = { {(PS8)"mode", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Mixedmode", (PS8)"Set mixed mode", (FuncToken_t) CuCmd_SetPrivacyMixedMode, aaa );
	}
	
	{
		ConParm_t aaa[]  = { {(PS8)"mode", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"aNywpamode", (PS8)"Set Any WPA mode", (FuncToken_t) CuCmd_SetPrivacyAnyWpaMode, aaa );
	}

	{
		ConParm_t aaa[]  = {
			{(PS8)"User:", CON_PARM_STRING, 0, WPACORE_MAX_CERT_USER_NAME_LENGTH, 0 },
			{(PS8)"Password:", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, WPACORE_MAX_CERT_PASSWORD_LENGTH , 0 },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h, (PS8)"Credentials", (PS8)"Set Credentials ", (FuncToken_t)CuCmd_SetPrivacyCredentials, aaa);
	}
#endif
	{                
		ConParm_t aaa[]  =
		{
			{(PS8)"Passphrase", CON_PARM_STRING, WPACORE_MIN_PSK_STRING_LENGTH, WPACORE_MAX_PSK_STRING_LENGTH, 0},                        
			{(PS8)"key type (hex | text) [text]", CON_PARM_OPTIONAL | CON_PARM_STRING, 0, 5, 0},
			CON_LAST_PARM
		};
		Console_AddToken(pTiCon->hConsole,h, (PS8)"pskPassphrase", (PS8)"Set PSK Passphrase", (FuncToken_t)CuCmd_SetPrivacyPskPassPhrase, aaa );
	}

#ifdef WPA_ENTERPRISE
	{
		ConParm_t aaa[]  = { {(PS8)"Certificate Name:", CON_PARM_STRING, 0, WPACORE_MAX_CERT_FILE_NAME_LENGTH, 0 },
		{(PS8)"Validate (yes - 1 /no - 0):", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"cerTificate", (PS8)"Set Certificate",(FuncToken_t)CuCmd_SetPrivacyCertificate, aaa);
		
	}
#endif
#ifdef WPA_SUPPLICANT
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Supplicant", (PS8)"Supplicant" ) );
	{
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Kill", (PS8)"Kill", (FuncToken_t) CuCmd_StopSuppl, NULL );
		
		ConParm_t aaa[]  =
		{
			{(PS8)"Level", CON_PARM_RANGE, 0, 4, 0 },
			{(PS8)"Show keys", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Show timestamps (yes - 1 /no - 0)", CON_PARM_RANGE, 0, 1, 0 },
			CON_LAST_PARM};
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Debug", (PS8)"Set debug", (FuncToken_t)CuCmd_ChangeSupplDebugLevels, aaa );
	}
#endif
	
	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Wep", (PS8)"Wep" ) );
	{
		ConParm_t aaa[]  =
		{
			{(PS8)"Key Value", CON_PARM_STRING, 0, 64, 0},
			{(PS8)"Tx Key Index", 0, 0, 0, 0 },
			{(PS8)"Default Key (yes - 1 /no - 0)", 0, 0, 0, 0 },
			{(PS8)"key type (hex | text) [hex]", CON_PARM_OPTIONAL | CON_PARM_STRING, 0, 5, 0},
			CON_LAST_PARM
		};
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Add", (PS8)"Add WEP", (FuncToken_t)CuCmd_AddPrivacyKey, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"Key Index", 0, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Remove", (PS8)"Remove WEP", (FuncToken_t)CuCmd_RemovePrivacyKey, aaa);
	}
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Get Default Key ID", (PS8)"Get Default Key ID", (FuncToken_t)CuCmd_GetPrivacyDefaultKey, NULL);
	
	
#ifdef XCC_MODULE_INCLUDED
	CuXCC_AddXCCMenu(pTiCon, h);
#endif/*XCC_MODULE_INCLUDED*/
	
    /* -------------------------------------------- Scan -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"scAn", (PS8)"Scan Service Configuration" ) );
    Console_AddToken(pTiCon->hConsole, h, (PS8)"Start", (PS8)"Start One-Shot Application Scan", (FuncToken_t) CuCmd_StartScan, NULL );
    Console_AddToken(pTiCon->hConsole, h, (PS8)"sTop", (PS8)"Stop One-Shot Application Scan", (FuncToken_t) CuCmd_StopScan, NULL );
#ifndef NO_WPA_SUPPL
    {
         ConParm_t aaa[]  = { {(PS8)"Scan Type (0=Active, 1=Passive)", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 1, 0 },
								{(PS8)"Ssid", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
                                 CON_LAST_PARM };

         Console_AddToken(pTiCon->hConsole,h, (PS8)"Wextstart", (PS8)"WEXT Start One-Shot Application Scan", (FuncToken_t) CuCmd_WextStartScan, aaa );
    }
#endif
    
    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"configApp", (PS8)"Configure One-Shot Application Scan Params" ) );
	{
		ConParm_t aaa[]  = {
			{(PS8)"SSID", CON_PARM_STRING, 0, 33, 0 },
#ifdef TI_DBG /* limitn application scan to normal only in release version */
			{(PS8)"Scan Type", CON_PARM_RANGE, SCAN_TYPE_NORMAL_PASSIVE, SCAN_TYPE_TRIGGERED_ACTIVE, 0 },
#else
			{(PS8)"Scan Type", CON_PARM_RANGE, SCAN_TYPE_NORMAL_PASSIVE, SCAN_TYPE_NORMAL_ACTIVE, 0 },
#endif
			{(PS8)"Band", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Probe Request Number", CON_PARM_RANGE, 0, 255, 0 },
			{(PS8)"Probe Request Rate", CON_PARM_RANGE, 0, DRV_RATE_MASK_54_OFDM, 0 },
			
#ifdef TI_DBG
			{(PS8)"Tid", CON_PARM_RANGE, 0, 255, 0 },
#endif
			{(PS8)"Number of Channels", CON_PARM_RANGE, 0, 16, 0 },
			CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h1, (PS8)"Global", (PS8)"Config Global Params", (FuncToken_t) CuCmd_ScanAppGlobalConfig, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Index", CON_PARM_RANGE, 0, 30, 0 },
			{(PS8)"BSSID (xx:xx:xx:xx:xx:xx)", CON_PARM_STRING, 0, 18, 0 },
			{(PS8)"Max Dwell Time", CON_PARM_RANGE, 0, 100000000, 0 },
			{(PS8)"Min Dwell Time", CON_PARM_RANGE, 0, 100000000, 0 },
			{(PS8)"ET Condition", CON_PARM_RANGE, SCAN_ET_COND_DISABLE, SCAN_ET_COND_ANY_FRAME, 0 },
			{(PS8)"ET Frame Number", CON_PARM_RANGE, 0, 255, 0 },
			{(PS8)"TX power level", CON_PARM_RANGE, 0, MAX_TX_POWER, 0 },
			{(PS8)"Channel Number", CON_PARM_RANGE, 0, 255, 0 },
			CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h1, (PS8)"Channel", (PS8)"Config Channel Params", (FuncToken_t) CuCmd_ScanAppChannelConfig, aaa );
	}
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"cLear", (PS8)"Clear All Params", (FuncToken_t) CuCmd_ScanAppClear, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Display", (PS8)"Display Params", (FuncToken_t) CuCmd_ScanAppDisplay, NULL );
	
    {
        ConParm_t aaa[]  = {
            {(PS8)"Aging threshold", CON_PARM_RANGE, 0, 1000, 60 } };
        Console_AddToken(pTiCon->hConsole,h1, (PS8)"Aging", (PS8)"Set aging threshiold", (FuncToken_t) CuCmd_ScanSetSra, aaa );
    }
    {
        ConParm_t aaa[]  = {
            {(PS8)"Rssi threshold", CON_PARM_RANGE | CON_PARM_SIGN, -100, 0, -80 } };
        Console_AddToken(pTiCon->hConsole,h1, (PS8)"Rssi", (PS8)"Set rssi threshiold", (FuncToken_t) CuCmd_ScanSetRssi, aaa );
    }

    CHK_NULL(h1 = (THandle) Console_AddDirExt (pTiCon->hConsole, (THandle)h, (PS8)"configpEriodic", (PS8)"Configure Periodic Application Scan" ) );
    {
        ConParm_t aaa[]  = {
            {(PS8)"RSSI Threshold", CON_PARM_RANGE | CON_PARM_SIGN, -100, 0, -97 },
            {(PS8)"SNR threshold", CON_PARM_RANGE | CON_PARM_SIGN, -10, 100, 0 },
            {(PS8)"Report threshold", CON_PARM_RANGE, 1, 8, 1 },
            {(PS8)"Terminate on report", CON_PARM_RANGE, 0, 1, 1 },
            {(PS8)"BSS Type (0-independent, 1-infrastructure, 2-any)", CON_PARM_RANGE, 0, 2, 2 },
            {(PS8)"Probe request number", CON_PARM_RANGE, 0, 5, 3 },
            {(PS8)"Number of scan cycles", CON_PARM_RANGE, 0, 100, 0 },
            {(PS8)"Number of SSIDs", CON_PARM_RANGE, 0, 8, 0 },
            {(PS8)"SSID List Filter Enabled", CON_PARM_RANGE, 0, 1, 1 },
            {(PS8)"Number of channels", CON_PARM_RANGE, 0, 32, 14 },
            CON_LAST_PARM };
        Console_AddToken (pTiCon->hConsole, h1, (PS8)"Global", (PS8)"Configure global periodic scan parameters", CuCmd_ConfigPeriodicScanGlobal, aaa);
    }
    {
        ConParm_t aaa[]  = {
            {(PS8)"Index", CON_PARM_RANGE, 0, PERIODIC_SCAN_MAX_INTERVAL_NUM - 1, 0 },
            {(PS8)"Interval (in millisec)", CON_PARM_RANGE, 0, 3600000, 1000 },
            CON_LAST_PARM };
        Console_AddToken (pTiCon->hConsole, h1, (PS8)"Interval", (PS8)"Configure interval table", CuCmd_ConfigPeriodicScanInterval, aaa);
    }
    {
        ConParm_t aaa[] = {
            {(PS8)"Index", CON_PARM_RANGE, 0, 7, 0 },
            {(PS8)"Visability (0-public, 1-hidden)", CON_PARM_RANGE, 0, 1, 0 },
            {(PS8)"SSID", CON_PARM_STRING, 0, 33, 0},
            CON_LAST_PARM };
        Console_AddToken (pTiCon->hConsole, h1, (PS8)"SSID", (PS8)"Configure SSID list", CuCmd_ConfigurePeriodicScanSsid, aaa);
    }
    {
        ConParm_t aaa[] = {
            {(PS8)"Index", CON_PARM_RANGE, 0, 32, 0 },
            {(PS8)"Band (0-2.4GHz, 1-5GHz)", CON_PARM_RANGE, 0, 1, 0 },
            {(PS8)"Channel", CON_PARM_RANGE, 0, 180, 0 },
            {(PS8)"Scan Type (0-passive, 1-active)", CON_PARM_RANGE, 0, 1, 0 },
            {(PS8)"Min dwell time (in millisec)", CON_PARM_RANGE, 1, 1000, 15 },
            {(PS8)"Max dwell time (in millisec)", CON_PARM_RANGE, 1, 1000, 100 },
            {(PS8)"TX power level (dBm*10)", CON_PARM_RANGE, 0, MAX_TX_POWER, 0 },
            CON_LAST_PARM };
        Console_AddToken (pTiCon->hConsole, h1, (PS8)"Channel", (PS8)"Configure channel list", CuCmd_ConfigurePeriodicScanChannel, aaa);
    }
    Console_AddToken (pTiCon->hConsole, h1, (PS8)"cLear", (PS8)"Clear configuration", CuCmd_ClearPeriodicScanConfiguration, NULL);
    Console_AddToken (pTiCon->hConsole, h1, (PS8)"Display", (PS8)"Display current configuration", CuCmd_DisplayPeriodicScanConfiguration, NULL);
    Console_AddToken (pTiCon->hConsole, h1, (PS8)"sTart", (PS8)"Start Periodic Scan", CuCmd_StartPeriodicScan, NULL);
    Console_AddToken (pTiCon->hConsole, h1, (PS8)"stoP", (PS8)"Stop periodic scan", CuCmd_StopPeriodicScan, NULL);

    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"configPolicy", (PS8)"Configure Scan Manager Scan Policy" ) );
	{
		ConParm_t aaa[]  = {
			{(PS8)"Normal scan interval (msec)", CON_PARM_RANGE, 0, 3600000, 5000 },
			{(PS8)"Deteriorating scan interval", CON_PARM_RANGE, 0, 3600000, 3000 },
			{(PS8)"Max Track Failures", CON_PARM_RANGE, 0, 20, 3 },
			{(PS8)"BSS list size", CON_PARM_RANGE, 0, 16, 8 },
			{(PS8)"BSS Number to start discovery", CON_PARM_RANGE, 0, 16, 4 },
			{(PS8)"Number of bands", CON_PARM_RANGE, 0, 2, 1 },
			CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h1, (PS8)"Gloabal", (PS8)"Set Global policy Params", (FuncToken_t) CuCmd_ConfigScanPolicy, aaa );
	}
	
	CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h1, (PS8)"Band", (PS8)"Configure band scan policy" ) );
	{
		ConParm_t aaa[]  = {
			{(PS8)"Index", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Band", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"RSSI threshold", CON_PARM_RANGE| CON_PARM_SIGN, -100, 0, 0 },
			{(PS8)"Channel number for discovery cycle", CON_PARM_RANGE, 0, 30, 5 },
			{(PS8)"Number of Channels", CON_PARM_RANGE, 0, 30, 0 },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"Misc", (PS8)"Set misc band params",  (FuncToken_t) CuCmd_ConfigScanBand, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Band Index", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Channel Index", CON_PARM_RANGE, 0, 29, 0 },
			{(PS8)"Channel", CON_PARM_RANGE, 0, 160, 0 },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"Channel", (PS8)"Set Channel params",  (FuncToken_t) CuCmd_ConfigScanBandChannel, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Band Index", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Scan Type", CON_PARM_RANGE, 0, 5, 0 },
			{(PS8)"ET event", CON_PARM_RANGE, SCAN_ET_COND_DISABLE, SCAN_ET_COND_ANY_FRAME, SCAN_ET_COND_DISABLE },
			{(PS8)"ET num of frames", CON_PARM_RANGE, 0, 255,0 },
			{(PS8)"Triggering AC", CON_PARM_RANGE, 0, 255, 0 },
			{(PS8)"Scan Duration (SPS)", CON_PARM_RANGE, 0, 100000000, 2000 },
			{(PS8)"Max dwell time", CON_PARM_RANGE, 0, 100000000, 60000 },
			{(PS8)"Min dwell time", CON_PARM_RANGE, 0, 100000000, 30000 },
			{(PS8)"Probe req. number", CON_PARM_RANGE, 0, 255, 2 },
			
			{(PS8)"Probe req. rate", CON_PARM_RANGE, 0, DRV_RATE_MASK_54_OFDM, 0 },
			
			{(PS8)"TX power level", CON_PARM_RANGE, 0, MAX_TX_POWER, 0 },
			CON_LAST_PARM };                
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"Track", (PS8)"Set tracking method params",  (FuncToken_t) CuCmd_ConfigScanBandTrack, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Band Index", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Scan Type", CON_PARM_RANGE, 0, 5, 0 },
			{(PS8)"ET event", CON_PARM_RANGE, SCAN_ET_COND_DISABLE, SCAN_ET_COND_ANY_FRAME, SCAN_ET_COND_DISABLE },
			{(PS8)"ET num of frames", CON_PARM_RANGE, 0, 255,0 },
			{(PS8)"Triggering AC", CON_PARM_RANGE, 0, 255, 0 },
			{(PS8)"Scan Duration (SPS)", CON_PARM_RANGE, 0, 100000000, 2000 },
			{(PS8)"Max dwell time", CON_PARM_RANGE, 0, 100000000, 60000 },
			{(PS8)"Min dwell time", CON_PARM_RANGE, 0, 100000000, 30000 },
			{(PS8)"Probe req. number", CON_PARM_RANGE, 0, 255, 2 },
			
			{(PS8)"Probe req. rate", CON_PARM_RANGE, 0, DRV_RATE_MASK_54_OFDM, 0 },
			
			{(PS8)"TX power level", CON_PARM_RANGE, 0, MAX_TX_POWER, 0 },
			CON_LAST_PARM };                
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"Discovery", (PS8)"Set Discovery method params",  (FuncToken_t) CuCmd_ConfigScanBandDiscover, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Band Index", CON_PARM_RANGE, 0, 1, 0 },
			{(PS8)"Scan Type", CON_PARM_RANGE, 0, 5, 0 },
			{(PS8)"ET event", CON_PARM_RANGE, SCAN_ET_COND_DISABLE, SCAN_ET_COND_ANY_FRAME, SCAN_ET_COND_DISABLE },
			{(PS8)"ET num of frames", CON_PARM_RANGE, 0, 255,0 },
			{(PS8)"Triggering AC", CON_PARM_RANGE, 0, 255, 0 },
			{(PS8)"Scan Duration (SPS)", CON_PARM_RANGE, 0, 100000000, 2000 },
			{(PS8)"Max dwell time", CON_PARM_RANGE, 0, 100000000, 60000 },
			{(PS8)"Min dwell time", CON_PARM_RANGE, 0, 100000000, 30000 },
			{(PS8)"Probe req. number", CON_PARM_RANGE, 0, 255, 2 },
			
			{(PS8)"Probe req. rate", CON_PARM_RANGE, 0, DRV_RATE_MASK_54_OFDM, 0 },
			
			{(PS8)"TX power level", CON_PARM_RANGE, 0, MAX_TX_POWER, 0 },
			CON_LAST_PARM };                
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"Immediate", (PS8)"Set Immediate method params",  (FuncToken_t) CuCmd_ConfigScanBandImmed, aaa );
	}
	
	
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Display", (PS8)"Display Policy Params", (FuncToken_t) CuCmd_DisplayScanPolicy, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"cLear", (PS8)"Clear Polciy Params", (FuncToken_t) CuCmd_ClearScanPolicy, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"Store", (PS8)"Send policy to scan manager", (FuncToken_t) CuCmd_SetScanPolicy, NULL );
	Console_AddToken(pTiCon->hConsole,h1, (PS8)"bsslisT", (PS8)"Display BSS list", (FuncToken_t) CuCmd_GetScanBssList, NULL );
	
	/* -------------------------------------------- roaminG -------------------------------------------- */
	
	/************ ROAMING manager commands - start  ********************/
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"roaminG", (PS8)"Roaming Manager configuration" ) );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Enable", (PS8)"Enable Internal Roaming", (FuncToken_t) CuCmd_RoamingEnable, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Disable", (PS8)"Disable Internal Roaming", (FuncToken_t) CuCmd_RoamingDisable, NULL );
	{
		ConParm_t aaa[]  = {
			{(PS8)"Low pass filter time", CON_PARM_RANGE, 0, 1440, 30 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h, (PS8)"Low pass filter", (PS8)"Time in sec ", (FuncToken_t) CuCmd_RoamingLowPassFilter, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Quality threshold", CON_PARM_RANGE | CON_PARM_SIGN, -150, 0, -70 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h, (PS8)"Quality threshold", (PS8)"Quality indicator", (FuncToken_t) CuCmd_RoamingQualityIndicator, aaa );
	}
	
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Get ", (PS8)"Get Roaming config params ", (FuncToken_t) CuCmd_RoamingGetConfParams, NULL );
	
    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Thresholds", (PS8)"Set Roaming MNGR triggers thresholds" ) );
	{
		ConParm_t aaa[]  = {
			{(PS8)"Tx retry", CON_PARM_RANGE, 0, 255, 20 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"Tx retry ", (PS8)"Consecutive number of TX retries", (FuncToken_t) CuCmd_RoamingDataRetryThreshold, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Bss loss", CON_PARM_RANGE, 1, 255, 4 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"Bss loss ", (PS8)"Number of TBTTs", (FuncToken_t) CuCmd_RoamingNumExpectedTbttForBSSLoss, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"tx Rate threshold", CON_PARM_RANGE, 0, 54, 2 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"tx Rate threshold ", (PS8)"TX rate (fallback) threshold", (FuncToken_t) CuCmd_RoamingTxRateThreshold, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Low rssi threshold", CON_PARM_RANGE | CON_PARM_SIGN, -150, 0, -80 }, CON_LAST_PARM };
			
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"low rssi thresHold ", (PS8)"Low RSSI threshold", (FuncToken_t) CuCmd_RoamingLowRssiThreshold, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"low Snr threshold", CON_PARM_RANGE, 0, 255, 10 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"low Snr threshold ", (PS8)"Low SNR threshold", (FuncToken_t) CuCmd_RoamingLowSnrThreshold, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"low Quality for scan", CON_PARM_RANGE | CON_PARM_SIGN, -150, -40, -85 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"low Quality for scan ", (PS8)"Increase the background scan", (FuncToken_t) CuCmd_RoamingLowQualityForBackgroungScanCondition, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Normal quality for scan", CON_PARM_RANGE | CON_PARM_SIGN, -150, -40, -70 }, CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"Normal quality for scan ", (PS8)"Reduce the background scan", (FuncToken_t) CuCmd_RoamingNormalQualityForBackgroungScanCondition, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"Index   ", CON_PARM_RANGE, 0, 1, 0  },
			{(PS8)"Threshold [dB / dBm] ", CON_PARM_RANGE | CON_PARM_SIGN, -100, 100, 0  },
			{(PS8)"Pacing    [Millisecond] ", CON_PARM_RANGE, 0, 60000, 1000  },
			{(PS8)"Metric    [0 - bcon_rssi, 1 - pkt_rssi, 2 - bcon_snr, 3 - pkt_snr] ", CON_PARM_RANGE, 0, 3, 0  },
			{(PS8)"Type      [0 - Level, 1 - Edge] ", CON_PARM_RANGE, 0, 1, 0  },
			{(PS8)"Direction [0 - Down, 1 - Up, 2 - Both] ", CON_PARM_RANGE, 0, 2, 0  },
			{(PS8)"Hystersis [dB] ", CON_PARM_RANGE, 0, 255, 0  },
			{(PS8)"Enable    [0 - Disable, 1 - Enable] ", CON_PARM_RANGE, 0, 1, 0  },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"User defined trigger\n", (PS8)"User defined FW trigger", (FuncToken_t) CuCmd_CurrBssUserDefinedTrigger, aaa );
	}
	
	/************ ROAMING manager commands - end  ********************/
	
    /* -------------------------------------------- QOS -------------------------------------------- */
	
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"qOs", (PS8)"Quality of service" ) );

    Console_AddToken(pTiCon->hConsole,h, (PS8)"aP params", (PS8)"Get AP QoS parameters", (FuncToken_t) CuCmd_GetApQosParams, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"ap Capabilities", (PS8)"Get AP QoS capabilities parameters", (FuncToken_t) CuCmd_GetApQosCapabilities, NULL );
    {
		ConParm_t ACid[]  = { {(PS8)"AC", CON_PARM_RANGE, 0, 3, 3  }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"ac Status", (PS8)"Get Current AC Status", (FuncToken_t) CuCmd_GetAcStatus, ACid );
	}
	Console_AddToken(pTiCon->hConsole,h, (PS8)"dEsired ps mode", (PS8)"Get desired PS mode", (FuncToken_t) CuCmd_GetDesiredPsMode, NULL );
    {
        ConParm_t aaa[]  = {
            {(PS8)"TID", CON_PARM_RANGE, 0, 7, 0  },
            {(PS8)"Stream Period (mSec)", CON_PARM_RANGE , 10, 100, 20 },
            {(PS8)"Tx Timeout (mSec)", CON_PARM_RANGE , 0, 200, 30 },
            {(PS8)"Enable", CON_PARM_RANGE , 0, 1, 1  },
            CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"set ps rX streaming", (PS8)"Set PS Rx Streaming", (FuncToken_t) CuCmd_SetPsRxDelivery, aaa );
    }
    Console_AddToken(pTiCon->hConsole,h, (PS8)"get ps rx streAming", (PS8)"Get PS Rx Streaming parameters", (FuncToken_t) CuCmd_GetPsRxStreamingParams, NULL );
    {
        ConParm_t aaa[]  = {
            {(PS8)"acID", CON_PARM_RANGE, 0, 3, 0  },
            {(PS8)"MaxLifeTime", CON_PARM_RANGE , 0, 1024, 0  },
            {(PS8)"PS Delivery Protocol (0 - Legacy, 1 - U-APSD)", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 1, 1 },
            CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"Qosparams", (PS8)"Set QOS Parameters", (FuncToken_t) CuCmd_SetQosParams, aaa );
    }
    {
        ConParm_t aaa[]  = {
            {(PS8)"PsPoll", CON_PARM_RANGE, 0, 65000, 0  },
            {(PS8)"UPSD", CON_PARM_RANGE , 0, 65000, 0  },
            CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"Rx TimeOut", (PS8)"Rx TimeOut", (FuncToken_t) CuCmd_SetRxTimeOut, aaa );
    }
    {       ConParm_t aaa[]  = {
        {(PS8)"Type", CON_PARM_RANGE, DSCP_CLSFR, CLSFR_TYPE_MAX, 0  },
        {(PS8)"D-Tag", CON_PARM_RANGE, CLASSIFIER_DTAG_MIN, CLASSIFIER_DTAG_MAX, CLASSIFIER_DTAG_DEF  },
        {(PS8)"Param1", CON_PARM_RANGE, 0, 65535, 0 },
        {(PS8)"Ip1", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0  },
        {(PS8)"Ip2", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        {(PS8)"Ip3", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        {(PS8)"Ip4", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        CON_LAST_PARM };
        Console_AddToken(pTiCon->hConsole,h, (PS8)"Insert class", (PS8)"Insert new classification entry", (FuncToken_t) CuCmd_InsertClsfrEntry, aaa );
    }
    {       ConParm_t aaa[]  = {
        {(PS8)"Type", CON_PARM_RANGE, DSCP_CLSFR, CLSFR_TYPE_MAX, 0  },
        {(PS8)"D-Tag", CON_PARM_RANGE, CLASSIFIER_DTAG_MIN, CLASSIFIER_DTAG_MAX, CLASSIFIER_DTAG_DEF  },
        {(PS8)"Param1", CON_PARM_RANGE, 0, 65535, 0 },
        {(PS8)"Ip1", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0  },
        {(PS8)"Ip2", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        {(PS8)"Ip3", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        {(PS8)"Ip4", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 255, 0 },
        CON_LAST_PARM };
        Console_AddToken(pTiCon->hConsole,h, (PS8)"remoVe class", (PS8)"Remove classification entry", (FuncToken_t) CuCmd_RemoveClsfrEntry, aaa );
    }

    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Tspec", (PS8)"TSPEC Sub-menu" ) );
	{       ConParm_t TspecParams[]  = {
		{(PS8)"UserPriority", CON_PARM_RANGE, 0, 7, 1  },
		{(PS8)"NominalMSDUsize", CON_PARM_RANGE, 1, 2312, 1  },
		{(PS8)"MeanDataRate (Bps units)", CON_PARM_RANGE, 0, 54000000, 0 },
		{(PS8)"MinimumPHYRate (Mbps units)", CON_PARM_RANGE , 0, 54, 0  },
		{(PS8)"SurplusBandwidthAllowance", CON_PARM_RANGE , 0, 7, 0 },
		{(PS8)"UPSD Mode (0 - Legacy, 1 - U-APSD)", CON_PARM_RANGE , 0, 1, 0 },
		{(PS8)"MinimumServiceInterval (usec)", CON_PARM_RANGE , 0, 1000000000, 0 },
		{(PS8)"MaximumServiceInterval (usec)", CON_PARM_RANGE , 0, 1000000000, 0 },
		CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Add", (PS8)"Add TSPEC", (FuncToken_t) CuCmd_AddTspec, TspecParams );
	}
	{
		ConParm_t UPid[]  = { {(PS8)"User priority", CON_PARM_RANGE, 0, 7, 1  }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Get", (PS8)"Get TSPEC Params", (FuncToken_t) CuCmd_GetTspec, UPid );
	}
	{
		ConParm_t UPid[]  = { {(PS8)"UserPriority", CON_PARM_RANGE, 0, 7, 1  }, 
		{(PS8)"ReasonCode", CON_PARM_RANGE, 32, 45, 32  }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Delete", (PS8)"Delete TSPEC", (FuncToken_t) CuCmd_DeleteTspec, UPid );
	}
	
	{
		ConParm_t MediumUsageParams[]  = {
			{(PS8)"AC", CON_PARM_RANGE, 0, 3, 3  },
			{(PS8)"HighThreshold", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 100, 1  },
			{(PS8)"LowThreshold", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 100, 1 },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"Medium usage", (PS8)"Medium usage threshold", (FuncToken_t) CuCmd_ModifyMediumUsageTh, MediumUsageParams );
	}
	
    
	/* -------------------------------------------- Power Management -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"poWer", (PS8)"Power Management" ) );
	{
		/* Set Power Mode Command */
		ConParm_t powerModeCmd[]  = {
			{(PS8)"PowerMode", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 3, 1 }, /* Min/Max/Def */
				CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h, (PS8)"set_Power_mode", (PS8)"Set user power mode", (FuncToken_t) CuCmd_SetPowerMode, powerModeCmd );
			
	}
	{
		/* Set Power Save Power level Command */
		ConParm_t powerSavePowerLevelCmd[]  = {
			{(PS8)"PowerSavePowerLevel", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 2, 2 }, /* Min/Max/Def */
				CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"set_powersave_powerLevel", (PS8)"Set the Power level during PowerSave", (FuncToken_t) CuCmd_SetPowerSavePowerLevel, powerSavePowerLevelCmd );
			
	}
	{
		/* Set default Power level Command */
		ConParm_t defaultPowerLevelCmd[]  = {
			{(PS8)"DefaultPowerLevel", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 2, 2 }, /* Min/Max/Def */
				CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"set_deFault_powerlevel", (PS8)"Set the default power level", (FuncToken_t) CuCmd_SetDefaultPowerLevel, defaultPowerLevelCmd );
			
	}
	{
		/* Set doze mode in auto power mode */
		ConParm_t powerSaveDozeMode[]  = {
			{(PS8)"DozeModeInAuto", CON_PARM_RANGE | CON_PARM_OPTIONAL, AUTO_POWER_MODE_DOZE_MODE_MIN_VALUE, AUTO_POWER_MODE_DOZE_MODE_MAX_VALUE, AUTO_POWER_MODE_DOZE_MODE_DEF_VALUE },
				CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h, (PS8)"set_doZe_mode_in_auto", (PS8)"Set doze mode in auto power mode", (FuncToken_t) CuCmd_SetDozeModeInAutoPowerLevel, powerSaveDozeMode );
			
	}
	
	{
		ConParm_t TrafficIntensityParams[]  = {
			{(PS8)"HighThreshold (packets/sec)", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 1000, 100  },
			{(PS8)"LowThreshold (packets/sec)", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 1000, 25 },
			{(PS8)"CheckInterval (ms)", CON_PARM_RANGE | CON_PARM_OPTIONAL, 100, 10000, 1000 },
			CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h, (PS8)"traffic_Thresholds", (PS8)"Set/Get traffic intensity thresholds", (FuncToken_t) CuCmd_SetTrafficIntensityTh, TrafficIntensityParams );
	}
	Console_AddToken(pTiCon->hConsole,h, (PS8)"eNable", (PS8)"enable traffic intensity events", (FuncToken_t) CuCmd_EnableTrafficEvents, NULL );
	Console_AddToken(pTiCon->hConsole,h, (PS8)"Disable", (PS8)"disable traffic intensity events", (FuncToken_t) CuCmd_DisableTrafficEvents, NULL );
	{
        /* set DCO-Itrim parameters */
		ConParm_t dcoItrimParams[]  = { {(PS8)"Enable/Disable <1/0>", CON_PARM_RANGE | CON_PARM_OPTIONAL, DCO_ITRIM_ENABLE_MIN, DCO_ITRIM_ENABLE_MAX, DCO_ITRIM_ENABLE_DEF },
                                        {(PS8)"Set Moderation Timeout (usec)", CON_PARM_RANGE | CON_PARM_OPTIONAL, DCO_ITRIM_MODERATION_TIMEOUT_MIN, DCO_ITRIM_MODERATION_TIMEOUT_MAX, DCO_ITRIM_MODERATION_TIMEOUT_DEF},
                                        CON_LAST_PARM };
        Console_AddToken(pTiCon->hConsole,h, (PS8)"set_dcO_itrim", (PS8)"Set/Get DCO Itrim Parameters", (FuncToken_t) CuCmd_SetDcoItrimParams, dcoItrimParams );
	}

	/* -------------------------------------------- Events -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"eVents", (PS8)"Events" ) );
	{
		ConParm_t aaa[]  = { {(PS8)"type", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Register", (PS8)"IPC events", (FuncToken_t)CuCmd_RegisterEvents, aaa);
	}
	{
		ConParm_t aaa[]  = { {(PS8)"type", CON_PARM_OPTIONAL, 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Unregister", (PS8)"IPC events", (FuncToken_t)CuCmd_UnregisterEvents, aaa);
	}

	/* -------------------------------------------- SG -------------------------------------------- */
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Bt CoExsistance", (PS8)"BT - Wlan CoExsistance" ) );
	{
		ConParm_t aaa[]  = { {(PS8)"enable", CON_PARM_RANGE | CON_PARM_OPTIONAL, 
			SOFT_GEMINI_ENABLED_MIN, SOFT_GEMINI_ENABLED_MAX, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Enable", (PS8)"Enable BT Coexistense", (FuncToken_t) CuCmd_EnableBtCoe, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"coexParamIdx", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			SOFT_GEMINI_PARAMS_INDEX_MIN, SOFT_GEMINI_PARAMS_INDEX_MAX, SOFT_GEMINI_PARAMS_INDEX_DEF  },
			{(PS8)"coexParamValue", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			SOFT_GEMINI_PARAMS_VALUE_MIN, SOFT_GEMINI_PARAMS_VALUE_MAX, SOFT_GEMINI_PARAMS_VALUE_DEF},
			CON_LAST_PARM };

			Console_AddToken(pTiCon->hConsole, h, (PS8)"Config", (PS8)"Parameters configuration", (FuncToken_t) CuCmd_ConfigBtCoe, aaa );
	}
    {
			ConParm_t aaa[]  = { {(PS8)"status", CON_PARM_RANGE | CON_PARM_OPTIONAL, 0, 3, 0 }, CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole, h, (PS8)"Status", (PS8)"Get status", (FuncToken_t) CuCmd_GetBtCoeStatus, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"coexIp", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			COEX_ACTIVITY_PARAMS_COEX_IP_MIN, COEX_ACTIVITY_PARAMS_COEX_IP_MAX, COEX_ACTIVITY_PARAMS_COEX_IP_DEF  },
			{(PS8)"activityId", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			COEX_ACTIVITY_PARAMS_ACTIVITY_ID_MIN, COEX_ACTIVITY_PARAMS_ACTIVITY_ID_MAX, COEX_ACTIVITY_PARAMS_ACTIVITY_ID_DEF},
			{(PS8)"defaultPriority", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_MIN, COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_MAX, COEX_ACTIVITY_PARAMS_DEFAULT_PRIO_DEF},
			{(PS8)"raisedPriority", CON_PARM_RANGE  | CON_PARM_OPTIONAL,
			COEX_ACTIVITY_PARAMS_RAISED_PRIO_MIN, COEX_ACTIVITY_PARAMS_RAISED_PRIO_MAX, COEX_ACTIVITY_PARAMS_RAISED_PRIO_DEF},
			{(PS8)"minService", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			COEX_ACTIVITY_PARAMS_MIN_SERVICE_MIN, COEX_ACTIVITY_PARAMS_MIN_SERVICE_MAX, COEX_ACTIVITY_PARAMS_MIN_SERVICE_DEF  },
			{(PS8)"maxService", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
			COEX_ACTIVITY_PARAMS_MAX_SERVICE_MIN, COEX_ACTIVITY_PARAMS_MAX_SERVICE_MAX, COEX_ACTIVITY_PARAMS_MAX_SERVICE_DEF},
			CON_LAST_PARM };

			Console_AddToken(pTiCon->hConsole, h, (PS8)"coexActivity", (PS8)"Coex Activity Parameters configuration", (FuncToken_t) CuCmd_ConfigCoexActivity, aaa );
	}
	{
		ConParm_t aaa[]  = {
			{(PS8)"enable", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_ENABLE_MIN, FM_COEX_ENABLE_MAX, FM_COEX_ENABLE_DEF  },
			{(PS8)"swallowPeriod", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_SWALLOW_PERIOD_MIN, FM_COEX_SWALLOW_PERIOD_MAX, FM_COEX_SWALLOW_PERIOD_DEF  },
			{(PS8)"nDividerFrefSet1", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_N_DIVIDER_FREF_SET1_MIN, FM_COEX_N_DIVIDER_FREF_SET1_MAX, FM_COEX_N_DIVIDER_FREF_SET1_DEF  },
			{(PS8)"nDividerFrefSet2", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_N_DIVIDER_FREF_SET2_MIN, FM_COEX_N_DIVIDER_FREF_SET2_MAX, FM_COEX_N_DIVIDER_FREF_SET2_DEF  },
			{(PS8)"mDividerFrefSet1", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_M_DIVIDER_FREF_SET1_MIN, FM_COEX_M_DIVIDER_FREF_SET1_MAX, FM_COEX_M_DIVIDER_FREF_SET1_DEF  },
			{(PS8)"mDividerFrefSet2", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_M_DIVIDER_FREF_SET2_MIN, FM_COEX_M_DIVIDER_FREF_SET2_MAX, FM_COEX_M_DIVIDER_FREF_SET2_DEF  },
			{(PS8)"pllStabilizationTime", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_PLL_STABILIZATION_TIME_MIN, FM_COEX_PLL_STABILIZATION_TIME_MAX, FM_COEX_PLL_STABILIZATION_TIME_DEF  },
			{(PS8)"ldoStabilizationTime", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_LDO_STABILIZATION_TIME_MIN, FM_COEX_LDO_STABILIZATION_TIME_MAX, FM_COEX_LDO_STABILIZATION_TIME_DEF  },
			{(PS8)"disturbedBandMargin", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_DISTURBED_BAND_MARGIN_MIN, FM_COEX_DISTURBED_BAND_MARGIN_MAX, FM_COEX_DISTURBED_BAND_MARGIN_DEF  },
			{(PS8)"swallowClkDif", CON_PARM_RANGE | CON_PARM_OPTIONAL ,
            FM_COEX_SWALLOW_CLK_DIF_MIN, FM_COEX_SWALLOW_CLK_DIF_MAX, FM_COEX_SWALLOW_CLK_DIF_DEF  },
			CON_LAST_PARM };

			Console_AddToken(pTiCon->hConsole, h, (PS8)"Fm_coexistence", (PS8)"FM Coexistence parameters configuration", (FuncToken_t) CuCmd_ConfigFmCoex, aaa );
	}
	
#ifdef XCC_MODULE_INCLUDED
	CuXCC_AddMeasurementMenu(pTiCon->hConsole);
#endif /* XCC_MODULE_INCLUDED*/

#ifdef TI_DBG
	
	/* -------------------------------------------- Report -------------------------------------------- */
	
    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"Report", (PS8)"Debug features" ) );
	{
		ConParm_t aaa[]  = 
		{ 
			{(PS8)"module table", CON_PARM_STRING | CON_PARM_OPTIONAL , REPORT_FILES_NUM, REPORT_FILES_NUM, 0 },
				CON_LAST_PARM };
            Console_AddToken(pTiCon->hConsole,h1, (PS8)"Set", (PS8)"set report module table", (FuncToken_t) CuCmd_SetReport, aaa );
		}
	{
		ConParm_t aaa[]  = 
		{ 
			{(PS8)"module", CON_PARM_OPTIONAL, 0, 0, 0 }, 
				CON_LAST_PARM 
		};
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Add", (PS8)"set report for specified module", (FuncToken_t) CuCmd_AddReport, aaa );
	}
	{
		ConParm_t aaa[]  = 
		{ 
			{(PS8)"module", CON_PARM_OPTIONAL, 0, 0, 0 }, 
				CON_LAST_PARM 
		};
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Clear", (PS8)"clear report for specified module", (FuncToken_t) CuCmd_ClearReport, aaa );
	}
	{
		ConParm_t aaa[]  = { {(PS8)"level", CON_PARM_OPTIONAL , 0, 0, 0 }, CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Level", (PS8)"set report severity level", (FuncToken_t) CuCmd_ReportSeverityLevel, aaa );
	}
	/* -------------------------------------------- Debug -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"dEbug", (PS8)"Debug features" ) );	
	{
		ConParm_t aaa[]  = {{(PS8)"func_num", CON_PARM_OPTIONAL, 0, 0, 0 },
							{(PS8)"param1 (decimal)", CON_PARM_OPTIONAL , 0, 0, 0 }, 
							{(PS8)"param2 (decimal)", CON_PARM_OPTIONAL , 0, 0, 0 }, 
							{(PS8)"R/W Mem buf (up to 32 characters)", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, FW_DEBUG_MAX_BUF * 2, 0 },
							CON_LAST_PARM };
		Console_AddToken(pTiCon->hConsole,h, (PS8)"Print", (PS8)"print driver debug info", (FuncToken_t) CuCmd_PrintDriverDebug, aaa );

      CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle)h , (PS8)"Fw Debug", (PS8)"Debug features" ) );
      {
       {
	      ConParm_t aaa[]  = { { (PS8)"debug", CON_PARM_OPTIONAL | CON_PARM_LINE, 0, 2050, 0 }, CON_LAST_PARM };
	      Console_AddToken(pTiCon->hConsole, h1, (PS8)"debug",  (PS8)" debug", (FuncToken_t) CuCmd_FwDebug, aaa );
	   }
      {
         ConParm_t aaa[]  = { {(PS8)"Index", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,0,255,0},
                              {(PS8)"Value_1", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_2", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_3", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_4", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_5", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_6", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_7", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_8", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_9", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_10", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_11", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_12", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  {(PS8)"Value_13", CON_PARM_OPTIONAL|CON_PARM_RANGE | CON_PARM_SIGN,-4096,4069,0},
							  CON_LAST_PARM };
          Console_AddToken(pTiCon->hConsole, h1, (PS8)"Set rate managment",  (PS8)"rate managment", (FuncToken_t) CuCmd_SetRateMngDebug, aaa );

      }
      {
         ConParm_t aaa[]  = { {(PS8)"Index", CON_PARM_OPTIONAL,0,4096,0},
                              {(PS8)"Value", CON_PARM_OPTIONAL,0,4096,0}, CON_LAST_PARM };
          Console_AddToken(pTiCon->hConsole, h1, (PS8)"Get rate managment",  (PS8)"rate managment", (FuncToken_t) CuCmd_GetRateMngDebug, aaa );
      }

      {
         ConParm_t aaa[]  = { {(PS8)"IpPart1", CON_PARM_OPTIONAL,0,255,0},
                              {(PS8)"IpPart2", CON_PARM_OPTIONAL,0,255,0},
                              {(PS8)"IpPart3", CON_PARM_OPTIONAL,0,255,0},
                              {(PS8)"IpPart4", CON_PARM_OPTIONAL,0,255,0},
                              CON_LAST_PARM };

          Console_AddToken(pTiCon->hConsole, h1, (PS8)"set Arp ip filter",  (PS8)"arp ip filter", (FuncToken_t) CuCmd_SetArpIPFilter, aaa );
      }

     }
    }

#endif /*TI_DBG*/
	
	/* -------------------------------------------- BIT -------------------------------------------- */
	
    CHK_NULL(h = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) NULL, (PS8)"biT", (PS8)"Built In Test" ) );

    CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Bip", (PS8)"Built In Production Line Test" ) );
	{
		{
			ConParm_t aaa[]  = {{(PS8)"iReferencePointDetectorValue", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 1000, 0 },
								{(PS8)"iReferencePointPower", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 200, 0 },
								{(PS8)"isubBand", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 10, 0 },
								CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"update Buffer calref point", (PS8)"BufferCalReferencePoint", (FuncToken_t) CuCmd_BIP_BufferCalReferencePoint, aaa );
		}

		{
			ConParm_t aaa[]  = {{(PS8)"Sub Band B/G:  1 - 14", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:    1 -  4", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:    8 - 16", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:   34 - 48", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:   52 - 64", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:  100 -116", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:  120 -140", CON_PARM_RANGE, 0, 1, 0 },
                                {(PS8)"Sub Band A:  149 -165", CON_PARM_RANGE, 0, 1, 0 },
								CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"Tx bip", (PS8)"P2G Calibration", (FuncToken_t) CuCmd_BIP_StartBIP, aaa );
		}

        
      CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h1, (PS8)"Rx bip", (PS8)"Rx Built In Production Line Test" ) );
      {
          {
           ConParm_t aaa[]  = {{(PS8)"initiates RX BIP operations",CON_PARM_OPTIONAL },
                              CON_LAST_PARM };
           Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx Enter", (PS8)"enter Rx Calibration state", (FuncToken_t) CuCmd_BIP_EnterRxBIP, aaa );
          }

          {
           ConParm_t aaa[]  = {{(PS8)"Reference point value", CON_PARM_SIGN },
                              CON_LAST_PARM };
           Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx Start", (PS8)" Rx Calibration", (FuncToken_t) CuCmd_BIP_StartRxBIP, aaa );
          }

          {
           ConParm_t aaa[]  = {{(PS8)"finished the RX BIP procedure" ,CON_PARM_OPTIONAL},
                              CON_LAST_PARM };
           Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx eXit", (PS8)"Exit Rx Calibration", (FuncToken_t) CuCmd_BIP_ExitRxBIP, aaa );
          }

       }
       
     
	}

	CHK_NULL(h1 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h, (PS8)"Radio debug", (PS8)"Radio Debug Test" ) );
	{
		/* Get HDK version*/
		Console_AddToken(pTiCon->hConsole,h1, (PS8)"Get hdk version",  (PS8)"HDK version", (FuncToken_t) CuCmd_RadioDebug_GetHDKVersion, NULL );
		/* Rx Channel Tune */
		{
			ConParm_t aaa[]  = {{(PS8)"Band", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 2, 0 },
								{(PS8)"Channel", CON_PARM_OPTIONAL|CON_PARM_RANGE , 1, 161, 0 }, 
								CON_LAST_PARM };
			Console_AddToken(pTiCon->hConsole,h1, (PS8)"cHannel tune", (PS8)"Set the RX channel", (FuncToken_t) CuCmd_RadioDebug_ChannelTune, aaa );
		}
		/* TX Debug */
		CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h1, (PS8)"Tx debug", (PS8)"TX Debug Test" ) );
		{
			/* TELEC */
			{
                ConParm_t aaa[]  = {{(PS8)"Power", CON_PARM_OPTIONAL, 0, 25000, 0 },
                                   {(PS8)"Tone Type", CON_PARM_OPTIONAL, 1, 2, 2 },
									/*	{(PS8)"Band", CON_PARM_OPTIONAL, 0, 2, 0 },
                                    {(PS8)"Channel", CON_PARM_OPTIONAL , 1, 161, 0 }, 
                                    {(PS8)"Power", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Tone Type", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Tone Number - Single Tones", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Tone Number - Two Tones", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Use Digital DC", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Invert", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Eleven N Span", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Digital DC", CON_PARM_OPTIONAL, 0, 0, 0 },
                                    {(PS8)"Analog DC Fine", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Analog DC Coarse", CON_PARM_OPTIONAL, 0, 0, 0 },*/
                                    CON_LAST_PARM };
				Console_AddToken(pTiCon->hConsole,h2, (PS8)"Cw", (PS8)"Start CW test", (FuncToken_t) CuCmd_RadioDebug_StartTxCw, aaa );

			}
			/* FCC */
			{
				ConParm_t aaa[]  = {{(PS8)"Delay", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Rate", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Size", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Amount", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Power", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Seed", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Packet Mode", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"DC On Off", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"GI", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Preamble", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Type", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Scrambler", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Enable CLPC", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Sequance Number Mode", CON_PARM_OPTIONAL, 0, 0, 0 },
									{(PS8)"Destination MAC Address", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
									/* future use. for now the oregenal source address are use.
                                     {(PS8)"Source MAC Address", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
                                     */
									{(PS8)"Destination MAC Address", CON_PARM_STRING | CON_PARM_OPTIONAL, 0, 32, 0 },
									CON_LAST_PARM };
				Console_AddToken(pTiCon->hConsole,h2, (PS8)"coNtinues", (PS8)"Start TX continues test", (FuncToken_t) CuCmd_RadioDebug_StartContinuousTx, aaa );
			}
			/* Stop FCC/TELEC */
			{
				Console_AddToken(pTiCon->hConsole,h2, (PS8)"Stop", (PS8)"Stop TX tests", (FuncToken_t) CuCmd_RadioDebug_StopTx, NULL );
			}
#if 0 /* not support for now */
			/* TEMPLATE */
			{
				ConParm_t aaa[]  = {{(PS8)"Length", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 0, 0 },
									{(PS8)"Offset", CON_PARM_OPTIONAL|CON_PARM_RANGE , 0, 0, 0 }, 
									{(PS8)"Data", CON_PARM_OPTIONAL|CON_PARM_RANGE, 0, 0, 0 },
									CON_LAST_PARM };
				Console_AddToken(pTiCon->hConsole,h2, (PS8)"temPlate", (PS8)"Set Template", (FuncToken_t) CuCmd_RadioDebug_Template, aaa );
			}
#endif
		}
		/*yael - to Complete statistics */
		CHK_NULL(h2 = (THandle) Console_AddDirExt(pTiCon->hConsole,  (THandle) h1, (PS8)"rx Statistics", (PS8)"Rx Statistics" ) );
		{
			/* RX Statixtics Start */
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx stat Start", (PS8)"Start Rx Statistics", (FuncToken_t)CuCmd_RadioDebug_StartRxStatistics , NULL );
			/* RX Statixtics Stop */
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx stat stoP", (PS8)"Stop Rx Statistics", (FuncToken_t)CuCmd_RadioDebug_StopRxStatistics , NULL );
			/* RX Statixtics Reset */
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx stat Reset",  (PS8)"Reset Rx Statistics", (FuncToken_t)CuCmd_RadioDebug_ResetRxStatistics , NULL );
			/* RX Statixtics Get */
			Console_AddToken(pTiCon->hConsole,h2, (PS8)"rx stat Get",  (PS8)"Get Rx Statistics", (FuncToken_t)CuCmd_RadioDebug_GetRxStatistics , NULL );
		}
	} /* h1 */

    /* -------------------------------------------- Root -------------------------------------------- */
				
	Console_AddToken(pTiCon->hConsole,NULL, (PS8)"aboUt", (PS8)"About", (FuncToken_t) CuCmd_ShowAbout, NULL );
	Console_AddToken(pTiCon->hConsole,NULL, (PS8)"Quit", (PS8)"quit", (FuncToken_t) CuCmd_Quit, NULL );

	return 0;
}


static S32 TiCon_Print_Usage(VOID)
{
    os_error_printf(CU_MSG_ERROR, (PS8)"Usage: ./wlan_cu [driver_name] [options]\n");    
    os_error_printf(CU_MSG_ERROR, (PS8)"   -b             - bypass supplicant\n");
	os_error_printf(CU_MSG_ERROR, (PS8)"   -i<ifname>     - supplicant interface file\n");
	os_error_printf(CU_MSG_ERROR, (PS8)"example:\n");
	os_error_printf(CU_MSG_ERROR, (PS8)"   ./wlan_cu tiwlan0 -i/voice/tiwlan0\n");
    return 0;
}

static VOID TiCon_SignalCtrlC(S32 signo)
{
	os_error_printf(CU_MSG_ERROR, (PS8)"TiCon_Signal - got signal Ctrl+c ... exiting\n");
	Console_Stop(g_TiCon.hConsole);
}


/* functions */
/*************/
S32 user_main(S32 argc, PS8* argv)
{
    S32 i;
    char *script_file = NULL;
    S32 BypassSupplicant = FALSE;
    S8 SupplIfFile[50];
    S32 fill_name = TRUE;
    int stop_UI = 0;

    SupplIfFile[0] = '\0';
    if( argc > 1 )
    {
        i=1;
        if( argv[i][0] != '-' )
        {
            os_strcpy( g_TiCon.drv_name, argv[i++] );
			fill_name = FALSE;
        }

        for( ;i < argc; i++ )
        {
            if( !os_strcmp(argv[i], (PS8)"-h" ) || !os_strcmp(argv[i], (PS8)"--help") )
            {
                TiCon_Print_Usage();
				return 0;
            }
            else if(!os_strcmp(argv[i], (PS8)"-b"))
            {
                BypassSupplicant = TRUE;
            }
            else if(!os_strncmp(argv[i], (PS8)"-i", 2))
            {
            	os_strcpy( SupplIfFile, &(argv[i])[2] );
            }
            else if(!os_strncmp(argv[i], "-s", 2 ) )
            {
                script_file = argv[++i];
            }
        }
    }

	os_OsSpecificCmdParams(argc, argv);

	/* fill the driver name */
    if(fill_name == TRUE)
    {
        os_strcpy(g_TiCon.drv_name, (PS8)TIWLAN_DRV_NAME);
    }

	/* fill supplicant interface file */
    if(SupplIfFile[0] == '\0')
    {
        os_strcpy(SupplIfFile, (PS8)SUPPL_IF_FILE);
    }

	g_TiCon.hConsole = Console_Create(g_TiCon.drv_name, BypassSupplicant, SupplIfFile);
	if(g_TiCon.hConsole == NULL)
		return 0;

	Console_GetDeviceStatus(g_TiCon.hConsole);	

	os_Catch_CtrlC_Signal(TiCon_SignalCtrlC);		

	os_InitOsSpecificModules();
    
    /* ----------------------------------------------------------- */
    TiCon_Init_Console_Menu(&g_TiCon);

	if( script_file )
    {
        stop_UI = consoleRunScript (script_file, g_TiCon.hConsole);
    }

    if( !stop_UI )
	{
        os_error_printf(CU_MSG_INFO2, (PS8)("user_main, start\n") );
		Console_Start(g_TiCon.hConsole);
	}

	Console_Destroy(g_TiCon.hConsole);

	os_DeInitOsSpecificModules();

    return 0;
}

/* Stubs for all OS */
void g_tester_send_event(U8 event_index)
{
}

void ProcessLoggerMessage(PU8 data, U32 len)
{
}
