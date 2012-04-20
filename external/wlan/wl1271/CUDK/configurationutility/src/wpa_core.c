/*
 * wpa_core.c
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
*   MODULE:  Wpa_Core.c
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

#ifdef ANDROID
#include <unistd.h>
#endif

#include <netinet/if_ether.h>

#include "cu_osapi.h"
#include "TWDriver.h"
#include "config_ssid.h"
#include "driver.h"
#include "ipc_wpa.h"
#include "wpa_core.h"
#include "oserr.h"

/* defines */
/***********/
#ifdef CONFIG_WPS
#define WSC_MODE_OFF	0
#define WSC_MODE_PIN	1
#define WSC_MODE_PBC	2
#endif

/* local types */
/***************/
/* Network configuration block - holds candidate connection parameters */
typedef struct 
{
	S32 mode;
	S32 proto;
	S32 key_mgmt;
	S32 auth_alg;
	S32 pair_wise;
	S32 group;
	U8 pass_phrase[WPACORE_MAX_PSK_STRING_LENGTH];
	U8 wep_key[4][32];
	U8 default_wep_key;
	U8 wep_key_length;
#ifdef CONFIG_WPS
	U8	WscMode;
	PS8 pWscPin;
#endif
    S32 eap;
    U8 Identity[WPACORE_MAX_CERT_PASSWORD_LENGTH];
    U8 private_key_passwd[WPACORE_MAX_CERT_PASSWORD_LENGTH];
    U8 private_key[WPACORE_MAX_CERT_PASSWORD_LENGTH];
    U8 client_cert[WPACORE_MAX_CERT_FILE_NAME_LENGTH];
    U8 password[WPACORE_MAX_CERT_PASSWORD_LENGTH];
    U8 anyWpaMode;
#ifdef XCC_MODULE_INCLUDED
    U16 XCC;
#endif
} TWpaCore_WpaSupplParams;

typedef struct 
{
	OS_802_11_AUTHENTICATION_MODE AuthMode;
	OS_802_11_ENCRYPTION_TYPES EncryptionTypePairWise;
	OS_802_11_ENCRYPTION_TYPES EncryptionTypeGroup;
} TWpaCore_WpaParams;

/* Module control block */
typedef struct TWpaCore
{
    THandle hIpcWpa;

    S32 CurrentNetwork;

	TWpaCore_WpaSupplParams WpaSupplParams; 
	TWpaCore_WpaParams WpaParams;
} TWpaCore;

/* local variables */
/*******************/

/* local fucntions */
/*******************/
static VOID WpaCore_InitWpaParams(TWpaCore* pWpaCore)
{
      os_memset( &pWpaCore->WpaSupplParams, 0, sizeof(TWpaCore_WpaSupplParams));
	  pWpaCore->WpaSupplParams.mode = IEEE80211_MODE_INFRA;         /* Default is Infrastructure mode */
	  pWpaCore->WpaSupplParams.proto = 0;                           /* key negotiation protocol - WPA is ok even though no security is used */ 
	  pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_NONE;        /* No key management suite */
	  pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;        /* Open authentication */
	  pWpaCore->WpaSupplParams.pair_wise = WPA_CIPHER_NONE;
	  pWpaCore->WpaSupplParams.group = WPA_CIPHER_NONE; 
      pWpaCore->WpaSupplParams.anyWpaMode = 0;
#ifdef CONFIG_WPS
	pWpaCore->WpaSupplParams.pWscPin = NULL;
	pWpaCore->WpaSupplParams.WscMode = WSC_MODE_OFF;
#endif

	pWpaCore->WpaParams.AuthMode = os802_11AuthModeOpen;
	pWpaCore->WpaParams.EncryptionTypeGroup = OS_ENCRYPTION_TYPE_NONE;
	pWpaCore->WpaParams.EncryptionTypePairWise = OS_ENCRYPTION_TYPE_NONE;
}

/* functions */
/*************/
THandle WpaCore_Create(PS32 pRes, PS8 pSupplIfFile)
{	
	TWpaCore* pWpaCore = (TWpaCore*)os_MemoryCAlloc(sizeof(TWpaCore), sizeof(U8));
	if(pWpaCore == NULL)
	{
		*pRes = OK;
		os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - WpaCore_Create - cant allocate control block\n");
		return NULL;
	}

    pWpaCore->hIpcWpa = IpcWpa_Create(pRes, pSupplIfFile);
	if(pWpaCore->hIpcWpa == NULL)
	{	
		WpaCore_Destroy(pWpaCore);
		return NULL;		
	}

	WpaCore_InitWpaParams(pWpaCore);

	pWpaCore->CurrentNetwork = -1;

	/* send default configuration to the supplicant */
	IpcWpa_Command(pWpaCore->hIpcWpa, (PS8)"AP_SCAN 2", FALSE);	

	return pWpaCore;
}

VOID WpaCore_Destroy(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	if(pWpaCore->hIpcWpa)
		IpcWpa_Destroy(pWpaCore->hIpcWpa);
#ifdef CONFIG_WPS
	if(pWpaCore->WpaSupplParams.pWscPin)
		os_MemoryFree(pWpaCore->WpaSupplParams.pWscPin);	
#endif

	os_MemoryFree(pWpaCore);
}

#ifdef XCC_MODULE_INCLUDED
S32 WpaCore_SetXCC(THandle hWpaCore, U16 XCCConfig)
{
    TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

    pWpaCore->WpaSupplParams.XCC = XCCConfig;

    return TI_OK;
}
#endif

S32 WpaCore_SetAuthMode(THandle hWpaCore, OS_802_11_AUTHENTICATION_MODE AuthMode)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	pWpaCore->WpaParams.AuthMode = AuthMode;

	switch (AuthMode)
	{
		case os802_11AuthModeOpen:
			pWpaCore->WpaSupplParams.proto = 0;
            pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_NONE;
            pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
            break;
		case os802_11AuthModeShared:
			pWpaCore->WpaSupplParams.proto = 0;
            pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_NONE;
            pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_SHARED;
            break;
		case os802_11AuthModeWPANone:
			pWpaCore->WpaSupplParams.proto = WPA_PROTO_WPA;
			pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_WPA_NONE;
			pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
			break;
		case os802_11AuthModeWPAPSK:
			pWpaCore->WpaSupplParams.proto = WPA_PROTO_WPA;
			pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_PSK;
			pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
			break;
		case os802_11AuthModeWPA2PSK:
			pWpaCore->WpaSupplParams.proto = WPA_PROTO_RSN;
			pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_PSK;
			pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
			break;
        case os802_11AuthModeWPA:
            pWpaCore->WpaSupplParams.proto = WPA_PROTO_WPA;
            pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_IEEE8021X;
            pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
            break;
       case os802_11AuthModeWPA2:
            pWpaCore->WpaSupplParams.proto = WPA_PROTO_RSN;
            pWpaCore->WpaSupplParams.key_mgmt = WPA_KEY_MGMT_IEEE8021X;
            pWpaCore->WpaSupplParams.auth_alg = WPA_AUTH_ALG_OPEN;
            break;

        default:
			os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - WpaCore_SetAuthMode - unknown authentication mode (%d)!!!\n", AuthMode);
            return ECUERR_WPA_CORE_ERROR_UNKNOWN_AUTH_MODE;
	}

	return OK;
}	


S32 WpaCore_GetAuthMode(THandle hWpaCore, PU32 pAuthMode)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	*pAuthMode = pWpaCore->WpaParams.AuthMode;

	return OK;
}	

S32 WpaCore_SetEncryptionPairWise(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES EncryptionType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	pWpaCore->WpaParams.EncryptionTypePairWise = EncryptionType;
	
	switch (EncryptionType)
    {
		case OS_ENCRYPTION_TYPE_NONE:
			pWpaCore->WpaSupplParams.pair_wise = WPA_CIPHER_NONE;
			break;
		case OS_ENCRYPTION_TYPE_WEP:
			pWpaCore->WpaSupplParams.pair_wise = WPA_CIPHER_WEP40;
			break;		
        case OS_ENCRYPTION_TYPE_TKIP:
            pWpaCore->WpaSupplParams.pair_wise = WPA_CIPHER_TKIP;
            break;
        case OS_ENCRYPTION_TYPE_AES:
			pWpaCore->WpaSupplParams.pair_wise = WPA_CIPHER_CCMP;
            break;
    }

	return OK;
}

S32 WpaCore_SetPrivacyEap(THandle hWpaCore, OS_802_11_EAP_TYPES EapType)
{
   TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

   pWpaCore->WpaSupplParams.eap = EapType;

   return OK;
}

S32 WpaCore_GetEncryptionPairWise(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES* pEncryptionType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
	
	*pEncryptionType = pWpaCore->WpaParams.EncryptionTypePairWise;

	return OK;
}

S32 WpaCore_SetEncryptionGroup(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES EncryptionType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	pWpaCore->WpaParams.EncryptionTypeGroup = EncryptionType;
	
	switch (EncryptionType)
    {			
    	case OS_ENCRYPTION_TYPE_NONE:
			os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - WpaCore_SetEncryptionGroup - group encryption cant be NONE\n");
			break;
		case OS_ENCRYPTION_TYPE_WEP:
			pWpaCore->WpaSupplParams.group = WPA_CIPHER_WEP40;
			break;
					
        case OS_ENCRYPTION_TYPE_TKIP:
            pWpaCore->WpaSupplParams.group = WPA_CIPHER_TKIP;
            break;
        case OS_ENCRYPTION_TYPE_AES:
			pWpaCore->WpaSupplParams.group = WPA_CIPHER_CCMP;
            break;
    }

	return OK;
}

S32 WpaCore_GetEncryptionGroup(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES* pEncryptionType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
	
	*pEncryptionType = pWpaCore->WpaParams.EncryptionTypeGroup;

	return OK;
}

S32 WpaCore_SetCredentials(THandle hWpaCore, PU8 Identity, PU8 Passward)
{
  TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

  os_memcpy((PVOID)pWpaCore->WpaSupplParams.Identity, (PVOID)Identity, os_strlen((PS8)Identity));

  if (Passward !=NULL)
  {
    os_memcpy((PVOID)pWpaCore->WpaSupplParams.password, (PVOID)Passward, os_strlen((PS8)Passward));
    os_memcpy((PVOID)pWpaCore->WpaSupplParams.private_key_passwd, (PVOID)Passward, os_strlen((PS8)Passward));
  }
 
  return OK;
}

S32 WpaCore_SetCertificate(THandle hWpaCore, PU8 Filepath)
{
  TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

  os_memcpy((PVOID)pWpaCore->WpaSupplParams.client_cert, (PVOID)Filepath, os_strlen((PS8)Filepath));

  return OK;

}

S32 WpaCore_SetPskPassPhrase(THandle hWpaCore, PU8 pPassPhrase)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
	
    os_memcpy((PVOID)pWpaCore->WpaSupplParams.pass_phrase, (PVOID)pPassPhrase, os_strlen((PS8)pPassPhrase));

	return OK;
}

S32 WpaCore_StopSuppl(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	IpcWpa_Command(pWpaCore->hIpcWpa, (PS8)"TERMINATE", TRUE);

	return OK;
}

S32 WpaCore_ChangeSupplDebugLevels(THandle hWpaCore, S32 Level1, S32 Level2, S32 Level3)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
    S8 cmd[100];

	os_sprintf(cmd, (PS8)"CHANGE_SUPPLICANT_DEBUG %ld %ld %ld", Level1, Level2, Level3);
	IpcWpa_Command(pWpaCore->hIpcWpa, cmd, TRUE);

	return OK;
}

S32 WpaCore_AddKey(THandle hWpaCore, OS_802_11_WEP* pKey)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
	U32 WepKeyIndx;

	WepKeyIndx = pKey->KeyIndex & 0x7FFFFFFF;
	
	if ((pKey->KeyIndex & 0x80000000) == 0x80000000)
	{
		/* Add "1" to the default wep key index - since "0" is used to indicate no default wep key */
		pWpaCore->WpaSupplParams.default_wep_key = WepKeyIndx + 1;
	}
	
	/* If key length wasn't set so far - set it according to current key */
	if (pWpaCore->WpaSupplParams.wep_key_length == 0)
	{
		pWpaCore->WpaSupplParams.wep_key_length = pKey->KeyLength;
	}
	else
	{
        if (pWpaCore->WpaSupplParams.wep_key_length != pKey->KeyLength) return ECUERR_WPA_CORE_ERROR_KEY_LEN_MUST_BE_SAME;     
	}
	
	os_memcpy(&pWpaCore->WpaSupplParams.wep_key[WepKeyIndx][0], pKey->KeyMaterial, pKey->KeyLength);

	return OK;
}

S32 WpaCore_GetDefaultKey(THandle hWpaCore, U32* pDefaultKeyIndex)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	*pDefaultKeyIndex = pWpaCore->WpaSupplParams.default_wep_key;

	return OK;
}

#ifdef CONFIG_WPS
S32 WpaCore_StartWpsPIN(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
#ifdef SUPPL_WPS_SUPPORT
	S8 cmd[100];
#endif

	pWpaCore->WpaSupplParams.WscMode = WSC_MODE_PIN;

#ifdef SUPPL_WPS_SUPPORT
	os_sprintf(cmd, "WPS_PIN any");
	IpcWpa_Command(pWpaCore->hIpcWpa, cmd, TRUE);
#endif

	return OK;
}

S32 WpaCore_StartWpsPBC(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
#ifdef SUPPL_WPS_SUPPORT
	S8 cmd[100];
#endif

	pWpaCore->WpaSupplParams.WscMode = WSC_MODE_PBC;

#ifdef SUPPL_WPS_SUPPORT
	os_sprintf(cmd, "WPS_PBC");
	IpcWpa_Command(pWpaCore->hIpcWpa, cmd, TRUE);
#endif

	return OK;
}

S32 WpaCore_StopWps(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	pWpaCore->WpaSupplParams.WscMode = WSC_MODE_OFF;
	
	return OK;
}

S32 WpaCore_SetPin(THandle hWpaCore, PS8 pPinStr)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
	int len = os_strlen(pPinStr);

	if (len == 0)
		return ECUERR_WPA_CORE_ERROR_IVALID_PIN;

	pWpaCore->WpaSupplParams.pWscPin = (PS8)os_MemoryCAlloc(len, sizeof(char));
	if(!pWpaCore->WpaSupplParams.pWscPin)
		return ECUERR_WPA_CORE_ERROR_CANT_ALLOC_PIN;

	os_strcpy(pWpaCore->WpaSupplParams.pWscPin, pPinStr);

	return OK;
}
#endif /* CONFIG_WPS */

S32 WpaCore_SetAnyWpaMode(THandle hWpaCore, U8 anyWpaMode)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	pWpaCore->WpaSupplParams.anyWpaMode = anyWpaMode;

	return OK;
}

S32 WpaCore_GetAnyWpaMode(THandle hWpaCore, PU8 pAnyWpaMode)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	*pAnyWpaMode = pWpaCore->WpaSupplParams.anyWpaMode;
	
	return OK;
}


S32 WpaCore_SetBssType(THandle hWpaCore, U32 BssType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	switch(BssType)
	{
		case os802_11IBSS:
			pWpaCore->WpaSupplParams.mode = IEEE80211_MODE_IBSS;
			break;
		case os802_11Infrastructure:
			pWpaCore->WpaSupplParams.mode = IEEE80211_MODE_INFRA;
			break;
		default:
			pWpaCore->WpaSupplParams.mode = IEEE80211_MODE_INFRA;
			break;		
	}

	return OK;
}

S32 WpaCore_GetBssType(THandle hWpaCore, PU32 pBssType)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;

	switch(pWpaCore->WpaSupplParams.mode)
	{
		case IEEE80211_MODE_IBSS:
			*pBssType = os802_11IBSS;
			break;
		case IEEE80211_MODE_INFRA:
			*pBssType = os802_11Infrastructure;
			break;
	}

	return OK;
}

S32 WpaCore_SetSsid(THandle hWpaCore, OS_802_11_SSID* ssid, TMacAddr bssid)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
    S8  cmd[256];
    S8  Resp[IPC_WPA_RESP_MAX_LEN];
    PS8 pRespTemp;
	U32 RespLen;
	U32 NetworkID;
	U32 SendCommand;
    U8  Len;
   
#define WPACORE_SETSSID_FAIL \
	os_error_printf(CU_MSG_ERROR, (PS8)"Failed to connect to %s\n", ssid->Ssid); \
    return ECUERR_WPA_CORE_ERROR_FAILED_CONNECT_SSID;

    /* First Add a new network block*/
	os_sprintf(cmd, (PS8)"ADD_NETWORK");
	if (IpcWpa_CommandWithResp(pWpaCore->hIpcWpa, cmd, TRUE, Resp, &RespLen))
	{
		WPACORE_SETSSID_FAIL;
	}
	NetworkID = os_strtoul(Resp, &pRespTemp, 0);

	/* Set the parameters in the new new network block*/

	/* Set the BSSID */
	if(!((bssid[0] == 0xFF) && 
		(bssid[1] == 0xFF) &&
		(bssid[2] == 0xFF) &&
		(bssid[3] == 0xFF) &&
		(bssid[4] == 0xFF) &&
		(bssid[5] == 0xFF)))
	{
		/* set th ebssid only if the bssid doesn't mean any bssid */	
        S8 temp[20];
		os_sprintf(temp, (PS8)"%02x", bssid[0]);
		os_sprintf(temp, (PS8)"%s:%02x", temp, bssid[1]);
		os_sprintf(temp, (PS8)"%s:%02x", temp, bssid[2]);
		os_sprintf(temp, (PS8)"%s:%02x", temp, bssid[3]);
		os_sprintf(temp, (PS8)"%s:%02x", temp, bssid[4]);
		os_sprintf(temp, (PS8)"%s:%02x", temp, bssid[5]);
		os_sprintf(cmd, (PS8)"SET_NETWORK %d bssid %s", NetworkID, temp);
		
		if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, FALSE))
		{
			WPACORE_SETSSID_FAIL;
		}
	}

   /* Set the SSID */
	os_sprintf(cmd, (PS8)"SET_NETWORK %d ssid \"%s\"", NetworkID, ssid->Ssid);
	if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;
	}

	/* set mode of the new network block */
	os_sprintf(cmd, (PS8)"SET_NETWORK %d mode %d", NetworkID, pWpaCore->WpaSupplParams.mode);
	if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		
	}

    /* set proto of the new network block */
	SendCommand = TRUE;
	if (pWpaCore->WpaSupplParams.proto == WPA_PROTO_WPA)
	{
		os_sprintf(cmd, (PS8)"SET_NETWORK %d proto WPA", NetworkID);
	}
	else if (pWpaCore->WpaSupplParams.proto == WPA_PROTO_RSN)
	{
		os_sprintf(cmd, (PS8)"SET_NETWORK %d proto RSN", NetworkID);
	}
	else
	{
		SendCommand = FALSE;
	}
	
	if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		
	}

	/* set key management of the new network block */
	SendCommand = TRUE;
	if (pWpaCore->WpaSupplParams.key_mgmt == WPA_KEY_MGMT_NONE)
	{
		if ((pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_LEAP) ||
			(pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_PEAP) ||
			(pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_FAST))
        {
         os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt IEEE8021X", NetworkID);
        }
		else
			os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt NONE", NetworkID);
	}
	else if (pWpaCore->WpaSupplParams.key_mgmt == WPA_KEY_MGMT_PSK)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt WPA-PSK", NetworkID);
	else if (pWpaCore->WpaSupplParams.key_mgmt == WPA_KEY_MGMT_IEEE8021X)
#ifdef XCC_MODULE_INCLUDED
       if((pWpaCore->WpaSupplParams.XCC == OS_XCC_CONFIGURATION_ENABLE_CCKM)||(pWpaCore->WpaSupplParams.XCC == OS_XCC_CONFIGURATION_ENABLE_ALL))
       {
        os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt WPA-EAP CCKM", NetworkID);
       }
       else
#endif
       os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt WPA-EAP", NetworkID);
   else if (pWpaCore->WpaSupplParams.key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d key_mgmt WPA-NONE", NetworkID);
	else
	{
		SendCommand = FALSE;
	}
	
	if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		
	}

   
	/* set authentication alog of the new network block */
	SendCommand = TRUE;
	if (pWpaCore->WpaSupplParams.auth_alg == WPA_AUTH_ALG_OPEN)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d auth_alg OPEN", NetworkID);
	else if (pWpaCore->WpaSupplParams.auth_alg == WPA_AUTH_ALG_SHARED)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d auth_alg SHARED", NetworkID);
	else if (pWpaCore->WpaSupplParams.auth_alg == WPA_AUTH_ALG_LEAP)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d auth_alg LEAP", NetworkID);
	else
	{
		SendCommand = FALSE;
	}
	
	if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		
	}


    /* set pairwise encryption of the new network block */
	SendCommand = TRUE;
	if (pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_NONE)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d pairwise NONE", NetworkID);
	else if (pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_WEP40)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d pairwise NONE", NetworkID);
	else if (pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_TKIP)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d pairwise TKIP", NetworkID);
	else if ((pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_CCMP)&& (pWpaCore->WpaSupplParams.anyWpaMode == 0))
		os_sprintf(cmd, (PS8)"SET_NETWORK %d pairwise CCMP", NetworkID);
    else if ((pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_CCMP)&& (pWpaCore->WpaSupplParams.anyWpaMode == 1))
		os_sprintf(cmd, (PS8)"SET_NETWORK %d pairwise CCMP TKIP", NetworkID);
	else
	{
		SendCommand = FALSE;
	}
	   
	if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		 
	}

   /* set group encryption of the new network block */
	SendCommand = TRUE;
	if (pWpaCore->WpaSupplParams.group == WPA_CIPHER_WEP40)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d group WEP40", NetworkID);
	else if (pWpaCore->WpaSupplParams.group == WPA_CIPHER_TKIP)
		os_sprintf(cmd, (PS8)"SET_NETWORK %d group TKIP", NetworkID);
	else if ((pWpaCore->WpaSupplParams.group == WPA_CIPHER_CCMP)&& (pWpaCore->WpaSupplParams.anyWpaMode == 0))
		os_sprintf(cmd, (PS8)"SET_NETWORK %d group CCMP", NetworkID);
    else if ((pWpaCore->WpaSupplParams.group == WPA_CIPHER_CCMP)&& (pWpaCore->WpaSupplParams.anyWpaMode == 1))
		os_sprintf(cmd, (PS8)"SET_NETWORK %d group CCMP TKIP", NetworkID);
	else
	{
		SendCommand = FALSE;
	}
	
	if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;		
	}

   
   /* set eap of the new network block */
    if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_NONE)
        SendCommand = FALSE;
    else
    {
      SendCommand = TRUE;
      if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_MD5_CHALLENGE)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap MD5", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_GENERIC_TOKEN_CARD)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap GTC", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_TLS)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap TLS", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_TTLS)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap TTLS", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_PEAP)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap PEAP", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_MS_CHAP_V2)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap MSCHAPV2", NetworkID);
#ifdef XCC_MODULE_INCLUDED
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_LEAP)
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap LEAP", NetworkID);
      else if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_FAST) 
        os_sprintf(cmd, (PS8)"SET_NETWORK %d eap FAST", NetworkID);
#endif
      else
       SendCommand = FALSE;
    }

   
    if (SendCommand && IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
    {
        WPACORE_SETSSID_FAIL;		
    }

     if (pWpaCore->WpaSupplParams.Identity[0]) 
	 {
       os_sprintf(cmd, (PS8)"SET_NETWORK %d identity \"%s\"", NetworkID,pWpaCore->WpaSupplParams.Identity);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	   {
          WPACORE_SETSSID_FAIL;  
	   }
     }
     
     if (pWpaCore->WpaSupplParams.client_cert[0])
	 {
       os_sprintf(cmd, (PS8)"SET_NETWORK %d client_cert \"%s\"", NetworkID,pWpaCore->WpaSupplParams.client_cert);
       
      if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
      {
       WPACORE_SETSSID_FAIL; 
      }
       
	 }

    
	 if (pWpaCore->WpaSupplParams.client_cert[0])
	 {

      Len = os_strlen ((PS8)pWpaCore->WpaSupplParams.client_cert);
      os_memcpy(pWpaCore->WpaSupplParams.private_key,pWpaCore->WpaSupplParams.client_cert,Len);
      os_memcpy(&pWpaCore->WpaSupplParams.private_key[Len-3],"pem",3);
    
      os_sprintf(cmd, (PS8)"SET_NETWORK %d private_key \"%s\"", NetworkID,pWpaCore->WpaSupplParams.private_key);
      if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
      {
        WPACORE_SETSSID_FAIL; 
      }
	 }
    
    if (pWpaCore->WpaSupplParams.private_key_passwd[0] ) 
	{
      os_sprintf(cmd, (PS8)"SET_NETWORK %d private_key_passwd \"%s\"", NetworkID,pWpaCore->WpaSupplParams.private_key_passwd);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
        {
         WPACORE_SETSSID_FAIL; 
        }
	}
    
    if (pWpaCore->WpaSupplParams.password[0] ) 
    {
      os_sprintf(cmd, (PS8)"SET_NETWORK %d password \"%s\"", NetworkID,pWpaCore->WpaSupplParams.password);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
        {
         WPACORE_SETSSID_FAIL; 
        }
      
    }


    if (pWpaCore->WpaSupplParams.eap == OS_EAP_TYPE_FAST) 
    {
      os_sprintf(cmd, (PS8)"SET_NETWORK %d phase1 \"fast_provisioning=3\"", NetworkID);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
          {
           WPACORE_SETSSID_FAIL; 
          }

     os_sprintf(cmd, (PS8)"SET_NETWORK %d pac_file \"/etc/wpa_supplicant.eap-fast-pac\"", NetworkID);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
          {
           WPACORE_SETSSID_FAIL; 
          }

     os_sprintf(cmd, (PS8)"SET_NETWORK %d anonymous_identity \"anonymous\"", NetworkID);
       if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
          {
           WPACORE_SETSSID_FAIL; 
          }

    }
        
    if (pWpaCore->WpaSupplParams.pair_wise == WPA_CIPHER_WEP40)	
	{
        S32 idx, idx2;
		
		for (idx=0; idx<4; idx++)
		{
            S8 TempBuf[3];
			os_sprintf(cmd, (PS8)"SET_NETWORK %d wep_key%d ", NetworkID, idx);
			for (idx2=0; idx2 < pWpaCore->WpaSupplParams.wep_key_length; idx2++)
			{
				os_sprintf(TempBuf, (PS8)"%02x", pWpaCore->WpaSupplParams.wep_key[idx][idx2]);
				os_strcat (cmd, TempBuf);
			}
			if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
			{
				WPACORE_SETSSID_FAIL;				
			}
		}
		os_sprintf(cmd, (PS8)"SET_NETWORK %d wep_tx_keyidx %d", NetworkID, pWpaCore->WpaSupplParams.default_wep_key-1);
		if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
		{
			WPACORE_SETSSID_FAIL;			
		}
	}

	if (pWpaCore->WpaSupplParams.key_mgmt == WPA_KEY_MGMT_PSK)
	{
		os_sprintf(cmd, (PS8)"SET_NETWORK %d psk \"%s\"", NetworkID, pWpaCore->WpaSupplParams.pass_phrase);
		if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
		{
			WPACORE_SETSSID_FAIL;			
		}
	}

   
#ifdef CONFIG_WPS
	if (pWpaCore->WpaSupplParams.WscMode)
	{
		os_sprintf(cmd, (PS8)"SET_NETWORK %d wsc_mode %d", NetworkID, pWpaCore->WpaSupplParams.WscMode);
		if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
		{
			WPACORE_SETSSID_FAIL;			
		}
	}
	if (pWpaCore->WpaSupplParams.pWscPin)
	{
		os_sprintf(cmd, (PS8)"SET_NETWORK %d wsc_pin \"%s\"", NetworkID, pWpaCore->WpaSupplParams.pWscPin);
		if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
		{
			WPACORE_SETSSID_FAIL;			
		}
	}
#endif

	/* Finaly Connect to the new network block */
	os_sprintf(cmd, (PS8)"SELECT_NETWORK %d", NetworkID);
	if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 0))
	{
		WPACORE_SETSSID_FAIL;
	}

	pWpaCore->CurrentNetwork = NetworkID;	
	IpcWpa_Command(pWpaCore->hIpcWpa, (PS8)"SAVE_CONFIG", 1);

   /* 
	init the connection params thus the next time we connect we will by default 
	connect to an open 
	*/
	WpaCore_InitWpaParams(pWpaCore);

	return OK;
}

S32 WpaCore_Disassociate(THandle hWpaCore)
{
	TWpaCore* pWpaCore = (TWpaCore*)hWpaCore;
    S8 cmd[256];

    os_sprintf(cmd, (PS8)"DISABLE_NETWORK %d", pWpaCore->CurrentNetwork);
    if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 1))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Failed to disconnect from current ssid\n");
        return ECUERR_WPA_CORE_ERROR_FAILED_DISCONNECT_SSID;
    }

    pWpaCore->CurrentNetwork = -1;
    IpcWpa_Command(pWpaCore->hIpcWpa, (PS8)"SAVE_CONFIG", 0);    

#if 0 /* for futur WPS work */
    if(pWpaCore->CurrentNetwork == -1)
    {
		os_sprintf(cmd, (PS8)"LIST_NETWORKS");
        if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 1))
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Failed to disconnect from current ssid\n");
            return ECUERR_WPA_CORE_ERROR_FAILED_DISCONNECT_SSID;
        }
    }
    else
    {   
        os_sprintf(cmd, (PS8)"DISABLE_NETWORK %d", pWpaCore->CurrentNetwork);
        if (IpcWpa_Command(pWpaCore->hIpcWpa, cmd, 1))
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Failed to disconnect from current ssid\n");
            return ECUERR_WPA_CORE_ERROR_FAILED_DISCONNECT_SSID;
        }

        pWpaCore->CurrentNetwork = -1;
        IpcWpa_Command(pWpaCore->hIpcWpa, (PS8)"SAVE_CONFIG", 0);    
    }
#endif /* #if 0 for futur WPS work */
 
    return OK;
}

