/*
 * cli_cu_common.h
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
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

#ifndef common_h
#define common_h

#include "tidef.h"

#ifdef _WINDOWS
#define DRV_NAME "\\\\.\\tiwlnpci"
#endif

#ifndef TIWLAN_DRV_NAME
#define TIWLAN_DRV_NAME  DRV_NAME
#endif

#define IN     /* input parameter          */
#define OUT    /* output parameter         */
#define INOUT  /* input & output parameter */

#ifndef TI_TRUE
#define TI_TRUE  1
#endif

#ifndef TI_FALSE
#define TI_FALSE 0
#endif

#ifndef TI_PENDING
#define TI_PENDING 2
#endif


#ifndef NULL
# define NULL 0L
#endif

#ifndef TI_VOIDCAST
#define TI_VOIDCAST(p) ((void)p)
#endif

#ifdef __LINUX__
#ifndef print
# define print(fmt, arg...) fprintf(stdout, fmt, ##arg)
#endif

#ifndef print_err
# define print_err(fmt, arg...) fprintf(stderr, fmt, ##arg)
#endif

#ifndef print_deb
# ifdef DEBUG_MESSAGES
#  define print_deb(fmt, arg...) fprintf(stdout, fmt, ##arg)
# else
#  define print_deb(fmt, arg...)
# endif	/* DEBUG_MESSAGES */
#endif	/* print_deb */

#endif /* __LINUX__ */

#ifndef SIZE_ARR
# define SIZE_ARR(a) (sizeof(a)/sizeof(a[0]) )
#endif

#ifndef min
# define min(a, b)	(((a)<(b)) ? (a) : (b))
#endif

#ifndef max
# define max(a, b)	(((a)>(b)) ? (a) : (b))
#endif

typedef struct
{
    TI_UINT32 value;
    char *name;
} named_value_t;

#define print_available_values(arr) \
        {   int i; for(i=0; i<SIZE_ARR(arr); i++) \
            print("%d - %s%s", arr[i].value, arr[i].name, (i>=SIZE_ARR(arr)-1) ? "\n" : ", " ); \
        }
        
void print_memory_dump(char *addr, int size );

#endif	/* common_h */

