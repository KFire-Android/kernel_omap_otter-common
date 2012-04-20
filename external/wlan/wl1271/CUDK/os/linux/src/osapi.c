/*
 * osapi.c
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

/****************************************************************************
*
*   MODULE:  osapi.c
*   
*   PURPOSE: 
* 
*   DESCRIPTION:  
*   ============
*   OS Memory API for user mode application (CUDK)      
*
****************************************************************************/

/* includes */
/************/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "cu_os.h"
#include "cu_osapi.h"

/* defines */
/***********/
#define MAX_HOST_MESSAGE_SIZE   512

S32 ipc_pipe[2];

extern S32 user_main (S32 argc, PPS8 argv);

/** 
 * \fn     main
 * \brief  Main entry point to a user-mode program 
 * 
 * This is the main() function for a user mode program, or the entry point
 * called by the OS, This calls an OS-abstracted main function
 * 
 * \param  argc - command line argument count
 * \param  argv - command line arguments
 * \return 0 on success, any other value indicates error 
 * \sa     user_main
 */ 
int main (int argc, char** argv)
{
    return user_main (argc, (PPS8)argv);
}

/****************************************************************************************
 *                        os_error_printf()                                 
 ****************************************************************************************
DESCRIPTION:    This function prints a debug message

ARGUMENTS:      OsContext   -   our adapter context.
                arg_list - string to output with arguments

RETURN:         None
*****************************************************************************************/
VOID os_error_printf(S32 debug_level, const PS8 arg_list ,...)
{
    static int g_debug_level = CU_MSG_ERROR; /* TODO ronen: create debug logic for CLI */
    S8 msg[MAX_HOST_MESSAGE_SIZE];
    va_list ap;
#ifdef OS_CLI_LOG_TO_FILE
    char file_name[30]="/cli.log";
    FILE *ftmp;
#endif

    if (debug_level < g_debug_level)
        return;

    /* Format the message */
    va_start(ap, arg_list);
    vsprintf((char *)msg, (char *)arg_list, ap);
    va_end(ap);

    /* print the message */
    fprintf(stderr, (char *)msg);

#ifdef OS_CLI_LOG_TO_FILE
    ftmp = fopen(file_name, "a");
    if (ftmp != NULL) {
        fprintf(ftmp,(char *)msg);
        fclose(ftmp);
    }
#endif
}

/****************************************************************************************
 *                        os_strcpy()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS strcpy fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
inline PS8 os_strcpy(PS8 dest, const PS8 src)
{
    return (PS8)strcpy((char*)dest, (char*)src);
}

/****************************************************************************************
 *                        os_strncpy()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS strncpy fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
PS8 os_strncpy(PS8 dest, const PS8 src, S32 n)
{
    return (PS8)strncpy((char*)dest, (char*)src, n);
}

/****************************************************************************************
 *                        os_sprintf()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS sprintf fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
S32 os_sprintf(PS8 str, const PS8 arg_list, ...)
{
    va_list ap;
    S8 msg[MAX_HOST_MESSAGE_SIZE];

    va_start(ap, arg_list);
    vsprintf((char*)msg, (char*)arg_list, ap);
    va_end(ap);

    return sprintf((char*)str, (char*)msg);
}

/****************************************************************************************
 *                        os_Printf()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS printf fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
S32 os_Printf(const PS8 buffer)
{
    return printf((char*)buffer);
}


/****************************************************************************************
 *                        os_strcat()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS strcat fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
PS8 os_strcat(PS8 dest, const PS8 src)
{
    return (PS8)strcat((char*)dest, (char*)src);
}

/****************************************************************************************
 *                        os_strlen()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS strlen fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
U32 os_strlen(const PS8 s)
{
    return strlen((char*)s);
}

/****************************************************************************************
 *                        os_memoryCAlloc()                                 
 ****************************************************************************************
DESCRIPTION:    Allocates an array in memory with elements initialized to 0.

ARGUMENTS:      OsContext   -   our adapter context.
                Number      -   Number of elements
                Size        -   Length in bytes of each element

RETURN:         Pointer to the allocated memory.
*****************************************************************************************/
PVOID os_MemoryCAlloc(U32 Number, U32 Size)
{
    return calloc(Number, Size);
}

/****************************************************************************************
 *                        os_memoryAlloc()                                 
 ****************************************************************************************
DESCRIPTION:    Allocates resident (nonpaged) system-space memory.

ARGUMENTS:      OsContext   - our adapter context.
                Size        - Specifies the size, in bytes, to be allocated.

RETURN:         Pointer to the allocated memory.
*****************************************************************************************/
PVOID os_MemoryAlloc(U32 Size)
{
    return malloc(Size);
}

/****************************************************************************************
 *                        os_memoryFree()                                 
 ****************************************************************************************
DESCRIPTION:    This function releases a block of memory previously allocated with the
                os_memoryAlloc function.


ARGUMENTS:      OsContext   -   our adapter context.
                pMemPtr     -   Pointer to the base virtual address of the allocated memory.
                                This address was returned by the os_memoryAlloc function.
                Size        -   Redundant, needed only for kernel mode.

RETURN:         None
*****************************************************************************************/
VOID os_MemoryFree(PVOID pMemPtr)
{
     free(pMemPtr);
}

/****************************************************************************************
 *                        os_memset()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS memset fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
PVOID os_memset(PVOID s, U8 c, U32 n)
{
    return memset(s, c, n);
}

/****************************************************************************************
 *                        os_memcpy()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS memcpy fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
PVOID os_memcpy(PVOID dest, const PVOID src, U32 n)
{
    return memcpy(dest, src, n);
}

/****************************************************************************************
 *                        os_memcmp()                                 
 ****************************************************************************************
DESCRIPTION:    wrapper to the OS memcmp fucntion

ARGUMENTS:      

RETURN:         
*****************************************************************************************/
S32 os_memcmp(const PVOID s1, const PVOID s2, S32 n)
{
    return memcmp(s1, s2, n);
}

/************************************************************************
 *                        os_strcmp                     *
 ************************************************************************
DESCRIPTION: wrapper to the OS strcmp fucntion

CONTEXT:  
************************************************************************/
S32 os_strcmp(const PS8 s1, const PS8 s2)
{
    return strcmp((char*)s1,(char*)s2);
}


/************************************************************************
 *                        os_strncmp                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS strcmp fucntion

CONTEXT:  
************************************************************************/
S32 os_strncmp(const PS8 s1, const PS8 s2, U32 n)
{
    return strncmp((char*)s1,(char*)s2,n);
}

/************************************************************************
 *                        os_sscanf                     *
 ************************************************************************
DESCRIPTION: wrapper to the OS sscanf fucntion

CONTEXT:  
************************************************************************/
S32 os_sscanf(const PS8 str, const PS8 arg_list, ...)
{
    va_list ap;
    S8 msg[MAX_HOST_MESSAGE_SIZE];

    va_start(ap, arg_list);
    vsprintf((char*)msg, (char*)arg_list, ap);
    va_end(ap);

    return sscanf((char*)str, (char*)msg);
}

/************************************************************************
 *                        os_strchr                     *
 ************************************************************************
DESCRIPTION: wrapper to the OS strchr fucntion

CONTEXT:  
************************************************************************/
PS8 os_strchr(const PS8 s, S32 c)
{
    return (PS8)strchr((char*)s,c);
}

/************************************************************************
 *                        os_strtol                     *
 ************************************************************************
DESCRIPTION: wrapper to the OS strtol fucntion

CONTEXT:  
************************************************************************/
S32 os_strtol(const PS8 nptr, PPS8 endptr, S32 base)
{
    return strtol((char*)nptr, (char**)endptr, base);
}

/************************************************************************
 *                        os_strtoul                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS strtoul fucntion

CONTEXT:  
************************************************************************/
U32 os_strtoul(const PS8 nptr, PPS8 endptr, S32 base)
{
    return strtoul((char*)nptr, (char**)endptr, base);
}

/************************************************************************
 *                        os_tolower                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS tolower fucntion

CONTEXT:  
************************************************************************/
S32 os_tolower(S32 c)
{
    return tolower(c);
}

/************************************************************************
 *                        os_tolower                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS tolower fucntion

CONTEXT:  
************************************************************************/
S32 os_isupper(S32 c)
{
    return isupper(c);
}

/************************************************************************
 *                        os_tolower                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS tolower fucntion

CONTEXT:  
************************************************************************/
S32 os_toupper(S32 c)
{
    return toupper(c);

}

/************************************************************************
 *                        os_atoi                        *
 ************************************************************************
DESCRIPTION: wrapper to the OS atoi fucntion

CONTEXT:  
************************************************************************/
S32 os_atoi(const PS8 str)
{
	return (S32)atoi(str);
}

/************************************************************************
 *                        os_fopen                      *
 ************************************************************************
DESCRIPTION: wrapper to the OS fopen fucntion

CONTEXT:  
************************************************************************/
PVOID os_fopen(const PS8 path, os_fopen_mode_e mode)
{
    switch(mode)
    {
        case OS_FOPEN_READ:
            return fopen((char*)path, "r");
		case OS_FOPEN_READ_BINARY:
			return fopen((char*)path, "rb");
		case OS_FOPEN_WRITE:
			return fopen(path, "w");
		case OS_FOPEN_WRITE_BINARY:
			return fopen(path, "wb");


        default:
            return NULL;
    }   
}

/************************************************************************
 *                        os_getFileSize                      *
 ************************************************************************
DESCRIPTION: wrapper to the OS fopen fucntion

CONTEXT:  
************************************************************************/
S32 os_getFileSize (PVOID file)
{
    S32 size;

    if (fseek(file, 0, SEEK_END))
    {
        os_error_printf (CU_MSG_ERROR, (PS8)"Cannot seek file to end\n");
        return -1;
    }
    size = ftell(file);
    rewind(file);  
    return size;
}

/************************************************************************
 *                        os_fgets                      *
 ************************************************************************
DESCRIPTION: wrapper to the OS fgets fucntion

CONTEXT:  
************************************************************************/
inline PS8 os_fgets(PS8 s, S32 size, PVOID stream)
{
    return (PS8)fgets((char*)s, size, stream);
}

/************************************************************************
 *                        os_fread                      *
 ************************************************************************
DESCRIPTION: wrapper to the OS fread fucntion

CONTEXT:  
************************************************************************/
inline S32 os_fread (PVOID ptr, S32 size, S32 nmemb, PVOID stream)
{
    return fread (ptr, size, nmemb, stream);
}

/************************************************************************
 *                        os_fwrite                      *
 ************************************************************************
DESCRIPTION: wrapper to the OS fwrite fucntion

CONTEXT:  
************************************************************************/
S32 os_fwrite (PVOID ptr, S32 size, S32 nmemb, PVOID stream)
{
    return fwrite (ptr, size, nmemb, stream);
}

/************************************************************************
 *                        os_fclose                     *
 ************************************************************************
DESCRIPTION: wrapper to the OS fclose fucntion

CONTEXT:  
************************************************************************/
inline S32 os_fclose(PVOID stream)
{
    return fclose(stream);
}

/************************************************************************
 *                        os_getInputString                     *
 ************************************************************************
DESCRIPTION: get the input string for the console from the appropiate inputs

CONTEXT:  
************************************************************************/
S32 os_getInputString(PS8 inbuf, S32 len)
{
    fd_set read_set; 
    S32 max_fd_index;
    S32 result;
    S32 pid;

    /*
     * Wait for one of two external events:
     * ----------------------------------- 
     *
     * 1. Data received from STDIN
     * 2. Data received from the event process
     */

    /* Prepare the read set fields */
    FD_ZERO(&read_set);
    FD_SET(0, &read_set);
    FD_SET(ipc_pipe[0], &read_set);

    /* Determine the maximum index of the file descriptor */
    max_fd_index = max(0, ipc_pipe[0]) + 1;
       
    /* Wait for event - blocking */
    result = select(max_fd_index, &read_set, NULL, NULL, NULL);

    if (result > 0)
    {
        if (FD_ISSET(0, &read_set))
        {
            /* Data received from STDIN */
            if ( fgets( (char*)inbuf, len, stdin ) == NULL )
                return FALSE;
            else
                return TRUE;
        } 
           
        if (FD_ISSET(ipc_pipe[0], &read_set))
        {
            /**********************************/
            /* Data received from TCP client */
            /********************************/
            result = read(ipc_pipe[0], inbuf, len);

            /* Get the pid of the calling process */
            pid = *(inbuf + 0) | (*(inbuf + 1) << 8);

            /* 
            Signal the calling process (tell him that we have 
            received the command, and he can send us another one 
            */
            if (pid != 0xFFFF)
            {
                kill(pid, SIGUSR1); 
            }

            if ( result <= 0 )
                return FALSE;
            else
                return TRUE;
        }
    }

    /* Error */
    os_error_printf(CU_MSG_ERROR, (PS8)"Input selection mismatch (0x%x)...\n", read_set);
    return FALSE;
}

/************************************************************************
 *                        os_Catch_CtrlC_Signal                     *
 ************************************************************************
DESCRIPTION: register to catch the Ctrl+C signal

CONTEXT:  
************************************************************************/
VOID os_Catch_CtrlC_Signal(PVOID SignalCB)
{
    if(signal(SIGINT, SignalCB) == SIG_ERR)
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - os_Catch_CtrlC_Signal - cant catch Ctrl+C signal\n");
}


VOID os_OsSpecificCmdParams(S32 argc, PS8* argv)
{
}

VOID os_InitOsSpecificModules(VOID)
{
}

VOID os_DeInitOsSpecificModules(VOID)
{
}

TI_SIZE_T os_get_last_error()
{
	return errno;
}
