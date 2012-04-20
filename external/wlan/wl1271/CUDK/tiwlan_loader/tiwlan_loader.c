/*
 * tiwlan_loader.c
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
 * \file  tiwlan_loader.c
 * \brief Loader implementation - sends FW image, NVS image and ini file to the driver
 */


#ifdef ANDROID
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <cutils/properties.h>
#include <hardware_legacy/power.h>
#define PROGRAM_NAME    "wlan_loader"
#endif

#include "STADExternalIf.h"
#include "cu_osapi.h"
#include "ipc_sta.h"
#include "WlanDrvCommon.h"

#define TIWLAN_DRV_NAME "tiwlan"

S8    g_drv_name[IF_NAME_SIZE + 1];

S32 print_usage(VOID)
{
    os_error_printf (CU_MSG_INFO1, (PS8)"Usage: ./wlan_loader [driver_name] [options]\n");
    os_error_printf (CU_MSG_INFO1, (PS8)"   -e <filename>  - eeprom image file name. default=./nvs_map.bin\n");
    os_error_printf (CU_MSG_INFO1, (PS8)"   -n - no eeprom file\n");
    os_error_printf (CU_MSG_INFO1, (PS8)"   -i <filename>  - init file name. default=tiwlan.ini\n");
    os_error_printf (CU_MSG_INFO1, (PS8)"   -f <filename>  - firmware image file name. default=firmware.bin\n");
    return 1;
}

/*  Return '0' if success */
S32 init_driver( PS8 adapter_name, PS8 eeprom_file_name,
                 PS8 init_file_name, PS8 firmware_file_name )
{
    PVOID f1 = NULL, f2 = NULL, f3 = NULL;
    S32 eeprom_image_length = 0;
    S32 init_file_length = 0;
    S32 firmware_image_length = 0;
    U32 req_size = 0;
    TLoaderFilesData *init_info = NULL;
    S32 rc = -1;
    THandle hIpcSta;

    if( !adapter_name || !*adapter_name )
        return rc;

    os_error_printf(CU_MSG_INFO1, (PS8)"+---------------------------+\n");
    os_error_printf(CU_MSG_INFO1, (PS8)"| wlan_loader: initializing |\n");
    os_error_printf(CU_MSG_INFO1, (PS8)"+---------------------------+\n");

    hIpcSta = IpcSta_Create(adapter_name);
    if (hIpcSta == NULL)
    {
	os_error_printf (CU_MSG_ERROR, (PS8)"wlan_loader: cant allocate IpcSta context\n", eeprom_file_name);
	goto init_driver_end;
    }

    /* Send init request to the driver */
    if ( (NULL != eeprom_file_name) &&
         (f1 = os_fopen (eeprom_file_name, OS_FOPEN_READ)) != NULL)
    {
        eeprom_image_length = os_getFileSize(f1);
        if (-1 == eeprom_image_length)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Cannot get eeprom image file length <%s>\n", eeprom_file_name);
            goto init_driver_end;
        }
    }

    if ( (NULL != firmware_file_name) &&
         (f2 = os_fopen (firmware_file_name, OS_FOPEN_READ)) != NULL)
    {
        firmware_image_length = os_getFileSize(f2);
        if (-1 == firmware_image_length)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Cannot get firmware image file length <%s>\n", firmware_file_name);
            goto init_driver_end;
        }
    }

    if ( (NULL != init_file_name) &&
         (f3 = os_fopen (init_file_name, OS_FOPEN_READ)) != NULL)
    {
        init_file_length = os_getFileSize(f3);
        if (-1 == init_file_length)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"Cannot get init file length <%s>\n", init_file_name);
            goto init_driver_end;
        }
    }

    /* Now when we can calculate the request length. allocate it and read the files */
    req_size = sizeof(TLoaderFilesData) + eeprom_image_length + (init_file_length+1) + firmware_image_length;
    init_info = (TLoaderFilesData *)os_MemoryAlloc(req_size);
    if (!init_info)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"No memory to allocate init request (%d bytes)\n", req_size);
        goto init_driver_end;
    }
    init_info->uNvsFileLength = eeprom_image_length;
    init_info->uFwFileLength  = firmware_image_length;
    init_info->uIniFileLength = init_file_length;

    if (!f1 || (eeprom_image_length &&
        os_fread(&init_info->data[0], 1, eeprom_image_length, f1)<eeprom_image_length))
    {
    } else
        os_error_printf(CU_MSG_INFO1, (PS8)"****  nvs file found %s **** \n", eeprom_file_name);

    if (!f2 || (firmware_image_length &&
        os_fread(&init_info->data[eeprom_image_length], 1, firmware_image_length, f2)<firmware_image_length))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Error reading firmware image %s - Aborting...\n", firmware_file_name);
        goto init_driver_end;
    }

    if (!f3 || (init_file_length &&
        os_fread(&init_info->data[eeprom_image_length+firmware_image_length], 1, init_file_length, f3)<init_file_length))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Warning: Error in reading init_file %s - Using defaults\n", init_file_name);
    }

    /* Load driver defaults */
    if(EOALERR_IPC_STA_ERROR_SENDING_WEXT == IPC_STA_Private_Send(hIpcSta, DRIVER_INIT_PARAM, init_info, req_size, NULL, 0))
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"Wlan_loader: Error sending init command (DRIVER_INIT_PARAM) to driver\n");
        goto init_driver_end;
    }

    /* No Error Found */
    rc = 0;

init_driver_end:
    if (f1)
        os_fclose(f1);
    if (f2)
        os_fclose(f2);
    if (f3)
        os_fclose(f3);
    if (init_info)
        os_MemoryFree(init_info);
    if (hIpcSta)
        IpcSta_Destroy(hIpcSta);

    return rc;
}

#ifdef ANDROID
int check_and_set_property(char *prop_name, char *prop_val)
{
    char prop_status[PROPERTY_VALUE_MAX];
    int count;

    for(count=4;( count != 0 );count--) {
        property_set(prop_name, prop_val);
        if( property_get(prop_name, prop_status, NULL) &&
            (strcmp(prop_status, prop_val) == 0) )
	    break;
    }
    if( count ) {
        os_error_printf(CU_MSG_ERROR, (PS8)"Set property %s = %s - Ok\n", prop_name, prop_val);
    }
    else {
        os_error_printf(CU_MSG_ERROR, (PS8)"Set property %s = %s - Fail\n", prop_name, prop_val);
    }
    return( count );
}
#endif

S32 user_main(S32 argc, PPS8 argv)
{
    S32 i;
    PS8 eeprom_file_name = (PS8)"./nvs_map.bin";
    PS8 init_file_name = (PS8)"tiwlan.ini";
    PS8 firmware_file_name = (PS8)"firmware.bin";

    /* Parse command line parameters */
    if( argc > 1 )
    {
        i=1;
        if( argv[i][0] != '-' )
        {
            os_strcpy( g_drv_name, argv[i++] );
        }
        for( ;i < argc; i++ )
        {
            if( !os_strcmp(argv[i], (PS8)"-h" ) || !os_strcmp(argv[i], (PS8)"--help") )
                return print_usage();
            else if(!os_strcmp(argv[i], (PS8)"-f" ) )
            {
                firmware_file_name = argv[++i];
            }
            else if(!os_strcmp(argv[i], (PS8)"-e") && (i+1<argc))
            {
                eeprom_file_name = argv[++i];
            }
            else if(!os_strcmp(argv[i], (PS8)"-i") && (i+1<argc))
            {
                init_file_name = argv[++i];
            }
            else if(!os_strcmp(argv[i], (PS8)"-n" ) )
            {
               eeprom_file_name = NULL;
            }
            else
            {
                os_error_printf (CU_MSG_ERROR, (PS8)"Loader: unknow parameter '%s'\n", argv[i]);
#ifdef ANDROID
                check_and_set_property("wlan.driver.status", "failed");
#endif
                return -1;
            }
        }
    }

    if( !g_drv_name[0] )
    {
        os_strcpy(g_drv_name, (PS8)TIWLAN_DRV_NAME "0" );
    }

#ifdef ANDROID
    acquire_wake_lock(PARTIAL_WAKE_LOCK, PROGRAM_NAME);
#endif

    if (init_driver (g_drv_name, eeprom_file_name, init_file_name, firmware_file_name) != 0)
    {
#ifdef ANDROID
        check_and_set_property("wlan.driver.status", "failed");
        release_wake_lock(PROGRAM_NAME);
#endif
        return -1;
    }
#ifdef ANDROID
    check_and_set_property("wlan.driver.status", "ok");
    release_wake_lock(PROGRAM_NAME);
#endif
    return 0;
}
