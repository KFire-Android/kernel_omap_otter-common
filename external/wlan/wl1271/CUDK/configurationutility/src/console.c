/*
 * console.c
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
*   MODULE:  console.c
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
#include <stdio.h>
#include "cu_osapi.h"
#include "console.h"
#include "cu_cmd.h"

/* defines */
/***********/
#define INBUF_LENGTH            2100
#define MAX_NAME_LEN        64
#define MAX_HELP_LEN        40
#define MAX_PARM_LEN        20
#define ALIAS_LEN           1

#define TOKEN_UP           ".."
#define TOKEN_ROOT         "/"
#define TOKEN_BREAK        "#"
#define TOKEN_HELP         "?"
#define TOKEN_DIRHELP      "help"
#define TOKEN_QUIT         "q1"

/* local types */
/***************/

typedef enum 
{ 
    Dir, 
    Token
} ConEntry_type_t;

/* Token types */
typedef enum
{
    EmptyToken,
    UpToken,
    RootToken,
    BreakToken,
    HelpToken,
    DirHelpToken,
    QuitToken,
    NameToken
} TokenType_t;


/* Monitor token structure */
typedef struct ConEntry_t
{
    struct ConEntry_t   *next;
    S8                  name[MAX_NAME_LEN+1];    /* Entry name */
    S8                  help[MAX_HELP_LEN+1];    /* Help string */
    PS8                 alias;                  /* Alias - always in upper case*/
    ConEntry_type_t     sel;                   /* Entry selector */
    
    union 
    {
        struct
        {
            struct ConEntry_t   *upper;            /* Upper directory */
            struct ConEntry_t   *first;            /* First entry */
        } dir;
        struct t_Token
        {
            FuncToken_t    f_tokenFunc;            /* Token handler */
            S32            totalParams;
            ConParm_t      *parm;                  /* Parameters array with totalParams size */
            PS8            *name;                 /* Parameter name with totalParams size */
        } token;
    } u;
} ConEntry_t;

/* Module control block */
typedef struct Console_t
{
    THandle hCuCmd;

    S32 isDeviceOpen;
    
    ConEntry_t *p_mon_root;
    ConEntry_t *p_cur_dir;
    PS8         p_inbuf;
    volatile S32 stop_UI_Monitor;
} Console_t;

/* local variables */
/*******************/

/* local fucntions */
/*******************/
static VOID Console_allocRoot(Console_t* pConsole);
int consoleRunScript( char *script_file, THandle hConsole);


/* Remove leading blanks */
static PS8 Console_ltrim(PS8 s)
{
    while( *s == ' ' || *s == '\t' ) s++;
    return s;
}

/* 
Make a preliminary analizis of <name> token.
Returns a token type (Empty, Up, Root, Break, Name)
*/
static TokenType_t Console_analizeToken( PS8 name )
{
    if (!name[0])
        return EmptyToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_UP ) )
        return UpToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_ROOT ) )
        return RootToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_BREAK ) )
        return BreakToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_HELP ) )
        return HelpToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_DIRHELP ) )
        return DirHelpToken;
    
    if (!os_strcmp(name, (PS8)TOKEN_QUIT ) )
        return QuitToken;

    return NameToken;
    
}

/* Compare strings case insensitive */
static S32 Console_stricmp( PS8 s1, PS8 s2, U16 len )
{
    S32  i;
    
    for( i=0; i<len && s1[i] && s2[i]; i++ )
    {
        if (os_tolower(s1[i])  != os_tolower(s2[i] ))
            break;
    }
    
    return ( (len - i) * (s1[i] - s2[i]) );
}

/* Convert string s to lower case. Return pointer to s */
static PS8 Console_strlwr( PS8 s )
{
    PS8 s0=s;
    
    while( *s )
    {
        *s = (S8)os_tolower(*s );
        ++s;
    }
    
    return s0;
}

/* free the entries tree */
static VOID Console_FreeEntry(ConEntry_t *pEntry)
{   
    ConEntry_t *pEntryTemp,*pEntryTemp1;
    
    if(pEntry->sel == Dir)
    {
        pEntryTemp = pEntry->u.dir.first;

        while (pEntryTemp)
        {
            pEntryTemp1 = pEntryTemp->next;
            Console_FreeEntry(pEntryTemp);
            pEntryTemp = pEntryTemp1;
        }
    }

    /* free the current entry */
    os_MemoryFree(pEntry);
}


/* Allocate root directory */
static VOID Console_allocRoot(Console_t* pConsole)
{   
    /* The very first call. Allocate root structure */
    if ((pConsole->p_mon_root=(ConEntry_t *)os_MemoryCAlloc(sizeof( ConEntry_t ), 1) ) == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)( "ERROR - Console_allocRoot(): cant allocate root\n") );
        return;
    }
    os_strcpy((PS8)pConsole->p_mon_root->name, (PS8)("\\") );
    pConsole->p_mon_root->sel = Dir;
    pConsole->p_cur_dir = pConsole->p_mon_root;
}

/* Display current directory */
static VOID Console_displayDir(Console_t* pConsole)
{
    S8 out_buf[512];
    ConEntry_t *p_token;
    ConEntry_t *p_dir = pConsole->p_cur_dir;

    os_sprintf((PS8)out_buf, (PS8)("%s%s> "), (PS8)(p_dir==pConsole->p_mon_root)? (PS8)("") : (PS8)(".../"), (PS8)p_dir->name );
    p_token = p_dir->u.dir.first;
    while( p_token )
    {
        if( (os_strlen(out_buf) + os_strlen(p_token->name) + 2)>= sizeof(out_buf) )
        {
            os_error_printf(CU_MSG_ERROR, ( (PS8)"ERROR - Console_displayDir(): buffer too small....\n") );
            break;
        }
        os_strcat(out_buf, p_token->name );
        if ( p_token->sel == Dir )
            os_strcat((PS8)out_buf, (PS8)("/" ) );
        p_token = p_token->next;
        if (p_token)
            os_strcat((PS8)out_buf, (PS8)(", ") );
    }
    
    os_error_printf(CU_MSG_INFO2, (PS8)("%s\n"), (PS8)out_buf );   
}


/* 
Cut the first U16 from <p_inbuf>.
Return the U16 in <name> and updated <p_inbuf>
*/
static TokenType_t Console_getWord(Console_t* pConsole, PS8 name, U16 len )
{
    U16         i=0;
    TokenType_t tType;  
    
    pConsole->p_inbuf = Console_ltrim(pConsole->p_inbuf);
    
    while( *pConsole->p_inbuf && *pConsole->p_inbuf!=' ' && i<len )
        name[i++] = *(pConsole->p_inbuf++);     
    
    if (i<len)
        name[i] = 0;
    
    tType   = Console_analizeToken( name );
    
    return tType;
}

static TokenType_t Console_getStrParam(Console_t* pConsole, PS8 buf, ConParm_t *param )
{
    TokenType_t tType;
    U32         i, len = param->hi_val;
    PS8         end_buf;
    
    pConsole->p_inbuf = Console_ltrim(pConsole->p_inbuf);
    
    if( param->flags & CON_PARM_LINE )
    {
        os_strcpy(buf, (PS8)pConsole->p_inbuf );
        pConsole->p_inbuf += os_strlen(pConsole->p_inbuf);
    }
    else
    {
        if( *pConsole->p_inbuf == '\"' )
        {
            end_buf = os_strchr(pConsole->p_inbuf+1, '\"' );
            if( !end_buf )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - invalid string param: '%s'\n"), (PS8)pConsole->p_inbuf );
                pConsole->p_inbuf += os_strlen(pConsole->p_inbuf);
                return EmptyToken;
            }
            if( (end_buf - pConsole->p_inbuf - 1) > (int)len )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - param is too long: '%s'\n"), (PS8)pConsole->p_inbuf );
                pConsole->p_inbuf += os_strlen(pConsole->p_inbuf);
                return EmptyToken;
            }
            *end_buf = 0;
            os_strcpy( buf, (PS8)(pConsole->p_inbuf+1 ) );
            pConsole->p_inbuf = end_buf + 1;
        }
        else
        {
            for( i=0; *pConsole->p_inbuf && *pConsole->p_inbuf!=' ' && i<len; i++ )
                buf[i] = *(pConsole->p_inbuf++);
            
            buf[i] = 0;
            if( *pConsole->p_inbuf && *pConsole->p_inbuf != ' ' )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - param is too long: '%s'\n"), (PS8)( pConsole->p_inbuf-os_strlen( buf) ) );
                pConsole->p_inbuf += os_strlen(pConsole->p_inbuf);
                return EmptyToken;
            }
        }
    }
    
    tType   = Console_analizeToken( buf );
    
    return tType;
}

/* Returns number of parameters of the given token
*/
static U16 Console_getNParms( ConEntry_t *p_token )
{
    U16 i;
    if ( !p_token->u.token.parm )
        return 0;
    for( i=0;
         (i<p_token->u.token.totalParams) &&
          p_token->u.token.parm[i].name &&
          p_token->u.token.parm[i].name[0];
         i++ )
        ;
    return i;
}

/* Parse p_inbuf string based on parameter descriptions in <p_token>.
Fill parameter values in <p_token>.
Returns the number of parameters filled.
To Do: add a option of one-by-one user input of missing parameters.
*/
static S32 Console_parseParms(Console_t* pConsole, ConEntry_t *p_token, U16 *pnParms )
{
    U16 nTotalParms = Console_getNParms( p_token );
    U16 nParms=0;
    PS8 end_buf = NULL;
    S8  parm[INBUF_LENGTH];
    U16 i, print_params = 0;
    U32 val = 0;
    S32 sval = 0;
    
    /* Mark all parameters as don't having an explicit value */
    for( i=0; i<nTotalParms; i++ )
        p_token->u.token.parm[i].flags |= CON_PARM_NOVAL;

    /*        -----------------              */
    pConsole->p_inbuf = Console_ltrim(pConsole->p_inbuf);
    if( pConsole->p_inbuf[0] == '!' && pConsole->p_inbuf[1] == '!' )
    {
        pConsole->p_inbuf += 2; print_params = 1;
    }
    /*        -----------------              */

    /* Build a format string */
    for( i=0; i<nTotalParms; i++ )
    {
        if (p_token->u.token.parm[i].flags & (CON_PARM_STRING | CON_PARM_LINE) )
        {
            /* For a string parameter value is the string address */
            /* and hi_val is the string length                   */
            if (Console_getStrParam(pConsole, parm, &p_token->u.token.parm[i] ) != NameToken)
                break;
            if( os_strlen( parm) > p_token->u.token.parm[i].hi_val ||
                (p_token->u.token.parm[i].low_val && p_token->u.token.parm[i].low_val > os_strlen( parm) ) )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - param '%s' must be %ld..%ld chars\n"), (PS8)p_token->u.token.parm[i].name,
                    (PS8)p_token->u.token.parm[i].low_val, (PS8)p_token->u.token.parm[i].hi_val);
                return FALSE;               
            }
            os_strcpy((PS8)(char *)p_token->u.token.parm[i].value, (PS8)parm);
        }
        else
        {
            if (Console_getWord(pConsole, parm, MAX_PARM_LEN ) != NameToken)
                break;
            
            if (p_token->u.token.parm[i].flags & CON_PARM_SIGN)
            {
                sval = os_strtol( parm, &end_buf, 0 );
            }
            else
            {
                val = os_strtoul( parm, &end_buf, 0 );
            }
            if( end_buf <= parm )
                break;

            /* Check value */
            if (p_token->u.token.parm[i].flags & CON_PARM_RANGE)
            {        
                if (p_token->u.token.parm[i].flags & CON_PARM_SIGN)
                {
                    if ((sval < (S32)p_token->u.token.parm[i].low_val) ||
                        (sval > (S32)p_token->u.token.parm[i].hi_val) )
                    {
                        os_error_printf(CU_MSG_ERROR, (PS8)("%s: %d out of range (%d, %d)\n"),
                            (PS8)p_token->u.token.parm[i].name, (int)sval,
                            (int)p_token->u.token.parm[i].low_val, (int)p_token->u.token.parm[i].hi_val );
                        return FALSE;
                    }
                    
                }
                else
                {                    
                    if ((val < p_token->u.token.parm[i].low_val) ||
                        (val > p_token->u.token.parm[i].hi_val) )
                    {
                        os_error_printf(CU_MSG_ERROR , (PS8)("%s: %ld out of range (%ld, %ld)\n"),
                            (PS8)p_token->u.token.parm[i].name, (PS8)val,
                            (PS8)p_token->u.token.parm[i].low_val, (PS8)p_token->u.token.parm[i].hi_val );
                        return FALSE;
                    }
                }
            }
            
            if (p_token->u.token.parm[i].flags & CON_PARM_SIGN)                
                p_token->u.token.parm[i].value = sval;
            else
                p_token->u.token.parm[i].value = val;
        }

        p_token->u.token.parm[i].flags &= ~CON_PARM_NOVAL;
        ++nParms;
    }

    /* Process default values */
    for( ; i<nTotalParms; i++ )
    {
        if ((p_token->u.token.parm[i].flags & CON_PARM_DEFVAL) != 0)
        {
            p_token->u.token.parm[i].flags &= ~CON_PARM_NOVAL;
            ++nParms;
        }
        else if (!(p_token->u.token.parm[i].flags & CON_PARM_OPTIONAL) )
        {
            /* Mandatory parameter missing */
            return FALSE;
        }
    }

    if( print_params )
    {
        os_error_printf((S32)CU_MSG_INFO2, (PS8)("Params: %d\n"), nParms );
        for (i=0; i<nParms; i++ )
        {
            os_error_printf(CU_MSG_INFO2, (PS8)("%d: %s - flags:%d"), 
                i+1, (PS8)p_token->u.token.parm[i].name,
                p_token->u.token.parm[i].flags);
            
            if (p_token->u.token.parm[i].flags & CON_PARM_SIGN)  
                os_error_printf(CU_MSG_INFO2, (PS8)("min:%d, max:%d, value:%d "),(PS8)p_token->u.token.parm[i].low_val, (PS8)p_token->u.token.parm[i].hi_val,
                (PS8)p_token->u.token.parm[i].value);
            else
                os_error_printf(CU_MSG_INFO2, (PS8)("min:%ld, max:%ld, value:%ld "),(PS8)p_token->u.token.parm[i].low_val, (PS8)p_token->u.token.parm[i].hi_val,
                (PS8)p_token->u.token.parm[i].value);
            
            os_error_printf(CU_MSG_INFO2, (PS8)("(%#lx)"),(PS8)p_token->u.token.parm[i].value );
            
            if( p_token->u.token.parm[i].flags & (CON_PARM_LINE | CON_PARM_STRING ))
            {
                os_error_printf(CU_MSG_INFO2, (PS8)(" - '%s'"), (PS8)(char *) p_token->u.token.parm[i].value );
            }
            os_error_printf(CU_MSG_INFO2, (PS8)("\n") );
        }
        
    }
    *pnParms = nParms;

    return TRUE;
}

/* Serach a token by name in the current directory */
static ConEntry_t *Console_searchToken( ConEntry_t *p_dir, PS8 name )
{
    ConEntry_t *p_token;
    U16        name_len = (U16)os_strlen( name );
    
    /* Check alias */
    p_token = p_dir->u.dir.first;
    while( p_token )
    {
        if (p_token->alias &&
            (name_len == ALIAS_LEN) &&
            !Console_stricmp( p_token->alias, name, ALIAS_LEN ) )
            return p_token;
        p_token = p_token->next;
    }
    
    /* Check name */
    p_token = p_dir->u.dir.first;
    while( p_token )
    {
        if (!Console_stricmp( p_token->name, name, name_len ) )
            break;
        p_token = p_token->next;
    }
    
    return p_token;
}


/* Display help for each entry in the current directory */
VOID  Console_dirHelp(Console_t* pConsole)
{
    ConEntry_t *p_token;
    S8        print_str[80];
    
    p_token = pConsole->p_cur_dir->u.dir.first;
    
    while( p_token )
    {
        if (p_token->sel == Dir)
            os_sprintf( print_str, (PS8)"%s: directory\n", (PS8)p_token->name );
        else
            os_sprintf( print_str, (PS8)("%s(%d parms): %s\n"),
            (PS8)p_token->name, Console_getNParms(p_token), p_token->help );
        os_error_printf(CU_MSG_INFO2,  (PS8)print_str );
        p_token = p_token->next;
    }
    
    os_error_printf(CU_MSG_INFO2, (PS8)("Type ? <name> for command help, \"/\"-root, \"..\"-upper\n") );
}


/* Display help a token */
static VOID  Console_displayHelp(Console_t* pConsole, ConEntry_t *p_token )
{
    S8 bra, ket;
    U16 nTotalParms = Console_getNParms( p_token );
    U16 i;
    
    
    os_error_printf(CU_MSG_INFO2, (PS8)("%s: %s "), (PS8)p_token->help, (PS8)p_token->name );
    for( i=0; i<nTotalParms; i++ )
    {
        if (p_token->u.token.parm[i].flags & CON_PARM_OPTIONAL)
        {
            bra = '['; ket=']';
        }
        else
        {
            bra = '<'; ket='>';
        }
        os_error_printf(CU_MSG_INFO2, (PS8)("%c%s"), bra, (PS8)p_token->u.token.parm[i].name );
        if (p_token->u.token.parm[i].flags & CON_PARM_DEFVAL)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)("=%lu"), (PS8)p_token->u.token.parm[i].value);
        }
        if (p_token->u.token.parm[i].flags & CON_PARM_RANGE)
        {
            os_error_printf(CU_MSG_INFO2, (PS8)(p_token->u.token.parm[i].flags & CON_PARM_SIGN) ? (PS8)(" (%d..%d%s)") : (PS8)(" (%lu..%lu%s)"),
                (PS8)p_token->u.token.parm[i].low_val,
                (PS8)p_token->u.token.parm[i].hi_val,
                (PS8)(p_token->u.token.parm[i].flags & (CON_PARM_STRING | CON_PARM_LINE)) ? (PS8)(" chars") : (PS8)("") );
            
        }
        os_error_printf(CU_MSG_INFO2, (PS8)("%c \n"),ket );
    }
}

/* Choose unique alias for <name> in <p_dir> */
/* Currently only single-character aliases are supported */
static S32 Console_chooseAlias( ConEntry_t *p_dir, ConEntry_t *p_new_token )
{
    ConEntry_t *p_token;
    S32         i;
    S8          c;
    PS8         new_alias = NULL;
    
    /* find alias given from user */
    for(i=0; p_new_token->name[i]; i++ )
    {
        if( os_isupper( p_new_token->name[i]) )
        {
            new_alias = &p_new_token->name[i];
            break;
        }
    }
    
    Console_strlwr( p_new_token->name );
    
    if( new_alias )
    {
        p_token = p_dir->u.dir.first;
        
        while( p_token )
        {
            if (p_token->alias && (os_tolower(*p_token->alias ) == *new_alias) )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("Error - duplicated alias '%c' in <%s> and <%s>**\n"), *new_alias,
                    (PS8)p_token->name, (PS8)p_new_token->name );
                return 0;
            }
            p_token = p_token->next;
        }
        *new_alias = (S8)os_toupper(*new_alias);
        p_new_token->alias = new_alias;
        return 1;
    }
    
    i = 0;
    while( p_new_token->name[i] )
    {
        c = p_new_token->name[i];
        p_token = p_dir->u.dir.first;
        
        while( p_token )
        {
            if (p_token->alias &&
                (os_tolower(*p_token->alias ) == c) )
                break;
            p_token = p_token->next;
        }
        if (p_token)
            ++i;
        else
        {
            p_new_token->name[i] = (S8)os_toupper( c );
            p_new_token->alias   = &p_new_token->name[i];
            break;
        }
    }
    return 1;
}

/* Parse the given input string and exit.
All commands in the input string are executed one by one.
*/
static U8 Console_ParseString(Console_t* pConsole, PS8 input_string )
{
    ConEntry_t  *p_token;
    S8          name[MAX_NAME_LEN];
    TokenType_t tType;
    U16         nParms; 
        
    if (!pConsole->p_mon_root)
        return 1;

    if(!pConsole->isDeviceOpen)
    {
        Console_GetDeviceStatus(pConsole);
        if(!pConsole->isDeviceOpen)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - Console_ParseString - Device isn't loaded !!!\n") );
            return 1;
        }
    }
    
    if( input_string[os_strlen( input_string)-1] == '\n' )
    {
        PS8 s = &input_string[os_strlen( input_string)-1];
        *s = 0;
    }
    pConsole->p_inbuf = input_string;
    pConsole->stop_UI_Monitor = FALSE;

    /* Interpret empty string as "display directory" */
    if ( pConsole->p_inbuf && !*pConsole->p_inbuf )
        Console_displayDir(pConsole);
    
    while(!pConsole->stop_UI_Monitor && pConsole->p_inbuf && *pConsole->p_inbuf)
    {
        tType = Console_getWord(pConsole, name, MAX_NAME_LEN );
        switch( tType )
        {
            
        case NameToken:
            p_token = Console_searchToken( pConsole->p_cur_dir, name );
            if (p_token == NULL)
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("**Error: '%s'**\n"),name);
                pConsole->p_inbuf = NULL;
            }
            else if (p_token->sel == Dir)
            {
                pConsole->p_cur_dir = p_token;
                Console_displayDir(pConsole);
            }
            else
            {  /* Function token */
                if (!Console_parseParms(pConsole, p_token, &nParms ))
                {
                    Console_displayHelp(pConsole, p_token );
                }
                else
                {
                    p_token->u.token.f_tokenFunc(pConsole->hCuCmd, p_token->u.token.parm, nParms );
                }
            }
            break;
            
        case UpToken: /* Go to upper directory */
            if (pConsole->p_cur_dir->u.dir.upper)
                pConsole->p_cur_dir = pConsole->p_cur_dir->u.dir.upper;
            Console_displayDir(pConsole);
            break;
            
        case RootToken: /* Go to the root directory */
            if (pConsole->p_cur_dir->u.dir.upper)
                pConsole->p_cur_dir = pConsole->p_mon_root;
            Console_displayDir(pConsole);
            break;
            
        case HelpToken: /* Display help */
            if (( Console_getWord(pConsole, name, MAX_NAME_LEN ) == NameToken ) &&
                ((p_token = Console_searchToken( pConsole->p_cur_dir, name )) != NULL ) &&
                (p_token->sel == Token) )
                Console_displayHelp(pConsole, p_token);
            else
                Console_dirHelp(pConsole);
            break;
            
        case DirHelpToken:
            Console_displayDir(pConsole);
            os_error_printf(CU_MSG_INFO2, (PS8)("Type ? <name> for command help, \"/\"-root, \"..\"-upper\n") );
            break;
            
        case BreakToken: /* Clear buffer */
            pConsole->p_inbuf = NULL;
            break;
            
        case QuitToken: /* Go to upper directory */
			return 1;

        case EmptyToken:
            break;
            
        }
    }
	return 0;
}

/* functions */
/*************/

THandle Console_Create(const PS8 device_name, S32 BypassSupplicant, PS8 pSupplIfFile)
{
    Console_t* pConsole = (Console_t*)os_MemoryCAlloc(sizeof(Console_t), sizeof(U8));
    if(pConsole == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)("Error - Console_Create - cant allocate control block\n") );
        return NULL;
    }

    pConsole->hCuCmd = CuCmd_Create(device_name, pConsole, BypassSupplicant, pSupplIfFile);
    if(pConsole->hCuCmd == NULL)
    {   
        Console_Destroy(pConsole);
        return NULL;
    }

    Console_allocRoot(pConsole);

    pConsole->isDeviceOpen = FALSE;

    return pConsole;
}

VOID Console_Destroy(THandle hConsole)
{
    Console_t* pConsole = (Console_t*)hConsole;

    if(pConsole->hCuCmd)
    {
        CuCmd_Destroy(pConsole->hCuCmd);
    }
    if (pConsole->p_mon_root)
    {
    Console_FreeEntry(pConsole->p_mon_root);
    }
    os_MemoryFree(pConsole);
}

VOID Console_Stop(THandle hConsole)
{   
    ((Console_t*)hConsole)->stop_UI_Monitor = TRUE;
}

/* Monitor driver */
VOID Console_Start(THandle hConsole)
{
    Console_t* pConsole = (Console_t*)hConsole;
    S8  inbuf[INBUF_LENGTH];
    S32 res;
        
    if (!pConsole->p_mon_root)
        return;

    pConsole->stop_UI_Monitor = FALSE;
    Console_displayDir(pConsole);
    
    while(!pConsole->stop_UI_Monitor)
    {
        /* get input string */
        res = os_getInputString(inbuf, sizeof(inbuf));
        if (res == FALSE) 
        {
            if(pConsole->stop_UI_Monitor)
            {
                continue;
            }
            else
            {
                return;
            }
        }   

        if(res == OS_GETINPUTSTRING_CONTINUE)
            continue;

        /* change to NULL terminated strings */
        if( inbuf[os_strlen(inbuf)-1] == '\n' )
            inbuf[os_strlen(inbuf)-1] = 0;
        
        /* parse the string */
        Console_ParseString(pConsole, inbuf);
    }

}

VOID Console_GetDeviceStatus(THandle hConsole)
{
    Console_t* pConsole = (Console_t*)hConsole;

    if(OK == CuCmd_GetDeviceStatus(pConsole->hCuCmd))
    {
        pConsole->isDeviceOpen = TRUE;
    }
}


/***************************************************************

  Function : consoleAddDirExt
  
    Description: Add subdirectory
    
      Parameters: p_root - root directory handle (might be NULL)
      name   - directory name
      
        Output:  the new created directory handle
        =NULL - failure
***************************************************************/
THandle Console_AddDirExt(THandle  hConsole,
                          THandle   hRoot,         /* Upper directory handle. NULL=root */
                          const PS8  name,          /* New directory name */
                          const PS8  desc )         /* Optional dir description */
{
    Console_t* pConsole = (Console_t*)hConsole;
    ConEntry_t *p_root = (ConEntry_t *)hRoot;
    ConEntry_t *p_dir;
    ConEntry_t **p_e;
    
    if (!p_root)
        p_root = pConsole->p_mon_root;
    
    if(!( p_root && (p_root->sel == Dir)))
        return NULL;
    
    if ( (p_dir=(ConEntry_t *)os_MemoryAlloc(sizeof( ConEntry_t )) ) == NULL)
        return NULL;
    
    os_memset( p_dir, 0, sizeof( ConEntry_t ) );
    os_strncpy( p_dir->name, name, MAX_NAME_LEN );
    os_strncpy( p_dir->help, desc, MAX_HELP_LEN );
    p_dir->sel = Dir;
    
    Console_chooseAlias( p_root, p_dir );
    
    /* Add new directory to the root's list */
    p_dir->u.dir.upper = p_root;
    p_e = &(p_root->u.dir.first);
    while (*p_e)
        p_e = &((*p_e)->next);
    *p_e = p_dir;
    
    return p_dir;
}

/***************************************************************

  Function : consoleAddToken
  
    Description: Add token
    
      Parameters: p_dir  - directory handle (might be NULL=root)
      name   - token name
      help   - help string
      p_func - token handler
      p_parms- array of parameter descriptions.
      Must be terminated with {0}.
      Each parm descriptor is a struct
      { "myname",         - name
      10,               - low value
      20,               - high value
      0 }               - default value =-1 no default
      or address for string parameter
      
        Output:  E_OK - OK
        !=0 - error
***************************************************************/
consoleErr Console_AddToken(  THandle hConsole,
                                THandle      hDir,
                                const PS8     name,
                                const PS8     help,
                                FuncToken_t   p_func,
                                ConParm_t     p_parms[] )
{
    Console_t* pConsole = (Console_t*)hConsole;
    ConEntry_t *p_dir = (ConEntry_t *)hDir;
    ConEntry_t *p_token;
    ConEntry_t **p_e;       
    U16       i;

    if (!pConsole->p_mon_root)
      Console_allocRoot(pConsole);

    if (!p_dir)
      p_dir = pConsole->p_mon_root;
    
    if(!( p_dir && (p_dir->sel == Dir)))
        return E_ERROR;
    
    
    /* Initialize token structure */
    if((p_token = (ConEntry_t *)os_MemoryCAlloc(1,sizeof(ConEntry_t))) == NULL)
    {
     os_error_printf(CU_MSG_ERROR, (PS8)("** no memory **\n") );
      return E_NOMEMORY;
    }
    
    
    /* Copy name */
    os_strncpy( p_token->name, name, MAX_NAME_LEN );
    os_strncpy( p_token->help, help, MAX_HELP_LEN );
    p_token->sel = Token;
    p_token->u.token.f_tokenFunc = p_func;
    p_token->u.token.totalParams = 0;
    
    /* Convert name to lower case and choose alias */
    Console_chooseAlias( p_dir, p_token );
    
    /* Copy parameters */
    if ( p_parms )
    {
       ConParm_t     *p_tmpParms = p_parms; 
       
       /* find the number of params */
       while( p_tmpParms->name && p_tmpParms->name[0] )
       {
            p_token->u.token.totalParams++;
            p_tmpParms++;
       }
       /* allocate the parameters info */
       p_token->u.token.parm = (ConParm_t *)os_MemoryAlloc(p_token->u.token.totalParams * sizeof(ConParm_t));
       p_token->u.token.name = (PS8*)os_MemoryAlloc(p_token->u.token.totalParams * sizeof(PS8));
       if ((p_token->u.token.parm == NULL) || (p_token->u.token.name == NULL))
       {
            os_error_printf(CU_MSG_ERROR, (PS8)("** no memory for params\n") );
            os_MemoryFree(p_token);
            return E_NOMEMORY;
       }
       for (i=0; i < p_token->u.token.totalParams; i++)
       {
         ConParm_t *p_token_parm = &p_token->u.token.parm[i];
    
         /* String parameter must have an address */
         if(p_parms->flags & (CON_PARM_STRING | CON_PARM_LINE))
         {
            if ( p_parms->hi_val >= INBUF_LENGTH )
            {
               os_error_printf(CU_MSG_ERROR, (PS8)("** buffer too big: %s/%s\n"), p_dir->name, name);
                os_MemoryFree(p_token->u.token.parm);
                os_MemoryFree(p_token->u.token.name);
                os_MemoryFree(p_token);
                return E_NOMEMORY;
    
            }
            if (p_parms->hi_val == 0 || (p_parms->flags & CON_PARM_RANGE) )
            {
               os_error_printf(CU_MSG_ERROR, (PS8)("** Bad string param definition: %s/%s\n"), p_dir->name, name );
                os_MemoryFree(p_token->u.token.parm);
                os_MemoryFree(p_token->u.token.name);
                os_MemoryFree(p_token);
                return E_BADPARM;
            }
            p_parms->value = (U32)os_MemoryCAlloc(1,p_parms->hi_val+1);
            if( !p_parms->value )
            {
                os_error_printf(CU_MSG_ERROR, (PS8)("** No memory: %s/%s (max.size=%ld)\n"), p_dir->name, name, p_parms->hi_val );
                os_MemoryFree(p_token->u.token.parm);
                os_MemoryFree(p_token->u.token.name);
                os_MemoryFree(p_token);
                return E_NOMEMORY;
            }
        }
    
        /* Copy parameter */
        *p_token_parm = *p_parms;
        if( p_token_parm->hi_val || p_token_parm->low_val )
            p_token_parm->flags |= CON_PARM_RANGE;
         
        p_token->u.token.name[i] = os_MemoryAlloc(os_strlen(p_parms->name));  
        if (p_token->u.token.name[i] == NULL)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)("** Error allocate param name\n"));
            os_MemoryFree(p_token->u.token.parm);
            os_MemoryFree(p_token->u.token.name);
            os_MemoryFree(p_token);
            return E_NOMEMORY;
        }
         p_token_parm->name = (PS8)p_token->u.token.name[i];
         os_strncpy( p_token->u.token.name[i], p_parms->name, os_strlen(p_parms->name) );
         ++p_parms;
      } /*end of for loop*/
    }
    
    /* Add token to the directory */
    p_e = &(p_dir->u.dir.first);
    while (*p_e)
      p_e = &((*p_e)->next);
    *p_e = p_token;
    
    return E_OK;
}

int consoleRunScript( char *script_file, THandle hConsole)
{
    FILE *hfile = fopen(script_file, "r" );
	U8 status = 0;
    Console_t* pConsole = (Console_t*)hConsole;

    if( hfile )
    {
        char buf[INBUF_LENGTH];
        pConsole->stop_UI_Monitor = FALSE;

        while( fgets(buf, sizeof(buf), hfile ) )
        {
            status = Console_ParseString(pConsole, buf);
			if (status == 1)
				return TRUE;
			if( pConsole->stop_UI_Monitor )
                break;
        }

        fclose(hfile);
    }
    else
	{
		os_error_printf(CU_MSG_ERROR, (PS8)("ERROR in script: %s \n"), (PS8)script_file);
	}
    return pConsole->stop_UI_Monitor;
}
