/*
 * stack.h
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

/** \file stack.h
 *  \brief Seport module API
 *
 *  \see stack.c
 */

/***************************************************************************/
/*                                                                         */
/*    MODULE:   stack.h                                                    */
/*    PURPOSE:  Stack module internal header API                           */
/*                                                                         */
/***************************************************************************/


#ifndef STACK_H
#define STACK_H


typedef struct _Stack_t
{
    TI_HANDLE   hOs;
    void       *pBuf;
    unsigned    uPtr;
    unsigned    uDep;
    unsigned    uElemSize;
    TI_BOOL        bBuf;

    void      (*fCpy) (TI_HANDLE, void*, void*, unsigned);

} Stack_t;



unsigned stackInit    (Stack_t *pStack, TI_HANDLE hOs, unsigned uElemSize, unsigned uDep, void *pBuf, void (*fCpy) (TI_HANDLE, void*, void*, unsigned));
unsigned stackDestroy (Stack_t *pStack);
unsigned stackPush    (Stack_t *pStack, void *pVal);
unsigned stackPop     (Stack_t *pStack, void *pVal);


#endif
