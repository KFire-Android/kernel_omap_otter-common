/*
 * console.h
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

/** \file  console.h 
 *  \brief Console (CLI) API
 *
 *  \see   console.c, ticon.c
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

/* defines */
/***********/
/* parameter flags */
#define CON_PARM_OPTIONAL       0x01  /* Parameter is optional */
#define CON_PARM_DEFVAL         0x02  /* Default value is set */
#define CON_PARM_RANGE          0x04  /* Range is set */
#define CON_PARM_STRING         0x08  /* String parm */
#define CON_PARM_LINE           0x10  /* String from the current parser position till EOL */
#define CON_PARM_SIGN           0x20  /* Signed param */
#define CON_PARM_NOVAL          0x80  /* Internal flag: parameter is anassigned */
#define CON_LAST_PARM           { NULL, 0, 0, 0, 0 }

#define CONSOLE_TERMINAL_MODULE_ID  (0)
#define CONSOLE_LOGGER_MODULE_ID    (1)
#define CONSOLE_WIPP_MODULE_ID      (2)
#define CONSOLE_G_TESTER_MODULE_ID  (3)
#define CONSOLE_NUMBER_OF_MODULES   (4)


/* types */
/*********/
typedef enum
{
    E_OK = 0, 
    E_BADPARM, 
    E_TOOMANY,
    E_NOMEMORY,
    E_NOT_FOUND,
    E_EXISTS,
    E_DUMMY,
    E_ERROR
} consoleErr;

typedef struct ConParm_t
{
   PS8         name;                     /* Parameter name. Shouldn't be allocated on stack! */
   U8          flags;                    /* Combination of CON_PARM_??? flags */
   U32         low_val;                  /* Low val for range checking */
   U32         hi_val;                   /* Hi val for range checking/max length of string */
   U32         value;                    /* Value/address of string parameter */
} ConParm_t;


typedef void (*FuncToken_t)(THandle hCuCmd, ConParm_t parm[], U16 nParms);


/* functions */
/*************/
THandle Console_Create(const PS8 device_name, S32 BypassSupplicant, PS8 pSupplIfFile);
VOID Console_Destroy(THandle hConsole);
VOID Console_GetDeviceStatus(THandle hConsole);
VOID Console_Start(THandle hConsole);
VOID Console_Stop(THandle hConsole);
THandle Console_AddDirExt( THandle  hConsole,
                            THandle  hRoot,
                            const PS8 name,
                            const PS8 desc );
consoleErr Console_AddToken( THandle hConsole,
                                THandle      hDir,
                                const PS8     name,
                                const PS8     help,
                                FuncToken_t   p_func,
                                ConParm_t     p_parms[] );

#endif  /* _CONSOLE_H_ */
        
