/*
 * oserr.h
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

/** 
 * \file  osErr.h 
 * \brief Declare error codes returned by OS abstraction
 */

#ifndef __OS_ERR_H__
#define __OS_ERR_H__

typedef enum
{
    EOALERR_NO_ERR = 0,
    EOALERR_CU_WEXT_ERROR_CANT_ALLOCATE = -1,
    EOALERR_IPC_STA_ERROR_SENDING_WEXT = -2,
    EOALERR_IPC_EVENT_ERROR_EVENT_NOT_DEFINED = -3,
    EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_ENABLED = -4,
    EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_DISABLED = -5,
    EOALERR_IPC_WPA_ERROR_CANT_CONNECT_TO_SUPPL = -6,
    EOALERR_IPC_WPA_ERROR_CMD_TIMEOUT = -7,
    EOALERR_IPC_WPA_ERROR_CMD_FAILED = -8,
    EOALERR_MAX_ERROR = EOALERR_IPC_WPA_ERROR_CMD_FAILED
} EOALError;

typedef enum
{
	ECUERR_CU_ERROR = 								EOALERR_MAX_ERROR - 1,
	ECUERR_CU_CMD_ERROR_DEVICE_NOT_LOADED = 		EOALERR_MAX_ERROR - 2,
	ECUERR_WPA_CORE_ERROR_UNKNOWN_AUTH_MODE = 		EOALERR_MAX_ERROR - 3,
	ECUERR_WPA_CORE_ERROR_KEY_LEN_MUST_BE_SAME = 	EOALERR_MAX_ERROR - 4,
	ECUERR_WPA_CORE_ERROR_FAILED_CONNECT_SSID = 	EOALERR_MAX_ERROR - 5,
	ECUERR_WPA_CORE_ERROR_FAILED_DISCONNECT_SSID= 	EOALERR_MAX_ERROR - 6,
	ECUERR_CU_COMMON_ERROR = 						EOALERR_MAX_ERROR - 7,
	ECUERR_WPA_CORE_ERROR_IVALID_PIN = 				EOALERR_MAX_ERROR - 8,
	ECUERR_WPA_CORE_ERROR_CANT_ALLOC_PIN = 			EOALERR_MAX_ERROR - 9
} ECUError;


#endif /* __OS_ERR_H__ */

