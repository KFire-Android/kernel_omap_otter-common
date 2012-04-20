/*
 * stack.c
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

/** \file stack.c
 *  \brief Seport module API
 *
 *  \see stack.h
 */

/***************************************************************************/
/*                                                                         */
/*    MODULE:   stack.c                                                    */
/*    PURPOSE:  Stack module implementation                                */
/*                                                                         */
/***************************************************************************/


#define __FILE_ID__  FILE_ID_133
#include "tidef.h"
#include "osApi.h"
#include "stack.h"


/**
 * \date 30-May-2006\n
 * \brief initialize stack object
 *
 * Function Scope \e Public.\n
 * \param pStack    - pointer to the Stack_t structure\n
 * \param hOS       - handle to the OS object\n
 * \param uElemSize - size of a one stack element\n
 * \param uDep      - stack depth\n
 * \param pBuf      - pointer to the stack buffer; if NULL a memory for the stack buffer will be dynamically allocated\n
 * \param fCpy      - pointer to function copying the stack element; if NULL a default copy function will be used\n
 * \return 0 - on success, -1 - on failure\n
 */
unsigned stackInit 
(
    Stack_t   *pStack, 
    TI_HANDLE  hOs,
    unsigned   uElemSize, 
    unsigned   uDep, 
    void      *pBuf, 
    void     (*fCpy) (TI_HANDLE, void*, void*, unsigned)
)
{
    pStack->hOs       = hOs;
    pStack->uPtr      = 0;
    pStack->uElemSize = uElemSize;
    pStack->uDep      = uDep * uElemSize;

    if (pBuf)
    {
        pStack->pBuf  = pBuf;
        pStack->bBuf  = 0;
    }

    else
    {
        pStack->pBuf  = _os_memoryAlloc (hOs, pStack->uDep);
        pStack->bBuf  = TI_TRUE;
    }

    if (fCpy)
        pStack->fCpy  = fCpy;
    else
        pStack->fCpy  = os_memoryCopy; 

    return 0; 
}


/**
 * \date 30-May-2006\n
 * \brief destroy stack object
 *
 * Function Scope \e Public.\n
 * \param pStack    - pointer to the Stack_t structure\n
 * \return 0 - on success, -1 - on failure\n
 */
unsigned stackDestroy (Stack_t *pStack)
{
    if (pStack->bBuf)
        _os_memoryFree (pStack->hOs, pStack->pBuf, pStack->uDep);

    return 0;
}


/**
 * \date 30-May-2006\n
 * \brief destroy stack object
 *
 * Function Scope \e Public.\n
 * \param pStack    - pointer to the Stack_t structure\n
 * \param pVal      - the pointer to the pushed value\n
 * \return 0 - on success, -1 - on failure\n
 */
unsigned stackPush (Stack_t *pStack, void *pVal)
{
    if (pStack->uPtr < pStack->uDep)
    {
        pStack->fCpy (pStack->hOs, (unsigned char*)pStack->pBuf + pStack->uPtr, pVal, pStack->uElemSize);
        pStack->uPtr += pStack->uElemSize;

        return 0;
    }

    return -1;
}


/**
 * \date 30-May-2006\n
 * \brief destroy stack object
 *
 * Function Scope \e Public.\n
 * \param pStack    - pointer to the Stack_t structure\n
 * \param pVal      - the pointer to the popped value\n
 * \return 0 - on success, -1 - on failure\n
 */
unsigned stackPop (Stack_t *pStack, void *pVal)
{
    if (pStack->uPtr > 0)
    {
        pStack->uPtr -= pStack->uElemSize;
        pStack->fCpy (pStack->hOs, pVal, (unsigned char*)pStack->pBuf + pStack->uPtr, pStack->uElemSize);

        return 0;
    }

    return -1;
}
