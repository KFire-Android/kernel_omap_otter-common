/*
 * wpa_core.h
 *
 * Copyright 2001-2009 Texas Instruments, Inc. - http://www.ti.com/
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

/****************************************************************************/
/*																			*/
/*    MODULE:   Wpa_core.h													*/
/*    PURPOSE:																*/
/*																			*/
/****************************************************************************/
#ifndef _WPA_CORE_H_
#define _WPA_CORE_H_

/* defines */
/***********/
#define WPACORE_MAX_CERT_FILE_NAME_LENGTH 		32
#define WPACORE_MAX_CERT_PASSWORD_LENGTH 		32
#define WPACORE_MAX_CERT_USER_NAME_LENGTH 		32

#define WPACORE_MAX_PSK_STRING_LENGTH		64
#define WPACORE_MIN_PSK_STRING_LENGTH		8
#define WPACORE_PSK_HEX_LENGTH				64

/* types */
/*********/

/* functions */
/*************/
THandle WpaCore_Create(PS32 pRes, PS8 pSupplIfFile);
VOID WpaCore_Destroy(THandle hWpaCore);

S32 WpaCore_SetAuthMode(THandle hWpaCore, OS_802_11_AUTHENTICATION_MODE AuthMode);
S32 WpaCore_GetAuthMode(THandle hWpaCore, PU32 pAuthMode);
S32 WpaCore_SetEncryptionPairWise(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES EncryptionType);
S32 WpaCore_GetEncryptionPairWise(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES* pEncryptionType);
S32 WpaCore_SetEncryptionGroup(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES EncryptionType);
S32 WpaCore_GetEncryptionGroup(THandle hWpaCore, OS_802_11_ENCRYPTION_TYPES* pEncryptionType);
S32 WpaCore_SetPskPassPhrase(THandle hWpaCore, PU8 pPassPhrase);
S32 WpaCore_StopSuppl(THandle hWpaCore);
S32 WpaCore_ChangeSupplDebugLevels(THandle hWpaCore, S32 Level1, S32 Level2, S32 Level3);
S32 WpaCore_AddKey(THandle hWpaCore, OS_802_11_WEP* pKey);
S32 WpaCore_GetDefaultKey(THandle hWpaCore, U32* pDefaultKeyIndex);
S32 WpaCore_SetAnyWpaMode(THandle hWpaCore, U8 anyWpaMode);
S32 WpaCore_GetAnyWpaMode(THandle hWpaCore, PU8 pAnyWpaMode);
S32 WpaCore_SetBssType(THandle hWpaCore, U32 BssType);
S32 WpaCore_GetBssType(THandle hWpaCore, PU32 pBssType);
S32 WpaCore_SetSsid(THandle hWpaCore, OS_802_11_SSID* ssid, TMacAddr bssid);
S32 WpaCore_Disassociate(THandle hWpaCore);
S32 WpaCore_SetPrivacyEap(THandle hWpaCore, OS_802_11_EAP_TYPES EapType);
S32 WpaCore_SetCredentials(THandle hWpaCore, PU8 Identity, PU8 Passward);
S32 WpaCore_SetCertificate(THandle hWpaCore, PU8 Filepath);
S32 WpaCore_SetXCC(THandle hWpaCore, U16 XCCConfig);


#ifdef CONFIG_WPS
S32 WpaCore_StartWpsPIN(THandle hWpaCore);
S32 WpaCore_StartWpsPBC(THandle hWpaCore);
S32 WpaCore_StopWps(THandle hWpaCore);
S32 WpaCore_SetPin(THandle hWpaCore, PS8 pPinStr);
#endif /* CONFIG_WPS */

#endif  /* _WPA_CORE_H_ */
        
