/*
 * osApi.h
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


/*--------------------------------------------------------------------------*/
/* Module:      OSAPI.H*/
/**/
/* Purpose:     This module defines unified interface to the OS specific*/
/*              sources and services.*/
/**/
/*--------------------------------------------------------------------------*/

#ifndef __OS_API_H__
#define __OS_API_H__

/** \file  osApi.h 
 * \brief Operating System APIs	\n
 * This module defines unified interface to the OS specific sources and services
 */

#include "tidef.h"
#include "TI_IPC_Api.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \struct TI_CONNECTION_STATUS
 * \struct *PTI_CONNECTION_STATUS
 * \brief Ti Connection Status
 * 
 * \par Description
 * 
 * \sa
 */ 
typedef struct 
{
    TI_UINT32      Event;
    TI_UINT8*      Data;
} TI_CONNECTION_STATUS, *PTI_CONNECTION_STATUS;

typedef struct 
{
    TI_UINT8 	uFormat;
    TI_UINT8 	uLevel;
    TI_UINT8 	uParamsNum;
    TI_UINT8 	uReserved;
    TI_UINT16 	uFileId;
    TI_UINT16 	uLineNum;
} TTraceMsg;

#define OS_PAGE_SIZE            4096
#define MAX_MESSAGE_SIZE        500
#define MICROSECOND_IN_SECONDS  1000000

#define UINT16_MAX_VAL					0xffff
#define UINT8_MAX_VAL					0xff

#define TRACE_FORMAT_8_BITS_PARAMS		2
#define TRACE_FORMAT_16_BITS_PARAMS		4
#define TRACE_FORMAT_32_BITS_PARAMS		6

#define TRACE_MSG_MAX_PARAMS			32
#define TRACE_MSG_MIN_LENGTH			(sizeof(TTraceMsg))
#define TRACE_MSG_MAX_LENGTH			((TRACE_MSG_MAX_PARAMS * 4) + sizeof(TTraceMsg))

#define INSERT_BYTE(pBuf, dataByte)     (*((TI_UINT8  *)pBuf) = (TI_UINT8 )dataByte ); pBuf++;
#define INSERT_2_BYTES(pBuf, dataBytes) (*((TI_UINT16 *)pBuf) = (TI_UINT16)dataBytes); pBuf+=2;
#define INSERT_4_BYTES(pBuf, dataBytes) (*((TI_UINT32 *)pBuf) = (TI_UINT32)dataBytes); pBuf+=4;


/****************************************************************************************
						START OF OS API (Common to all GWSI LIB, Driver and TI Driver)				
*****************************************************************************************/


/****************************************************************************************
                        OS HW API NEEDED BY DRIVER              
*****************************************************************************************/

/** \brief  OS Disable IRQ
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return void
 * 
 * \par Description
 * This function disables the Interrupts
 * 
 * \sa
 */ 
void os_disableIrq (TI_HANDLE OsContext);

/** \brief  OS Enable IRQ
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return void
 * 
 * \par Description
 * This function enables the Interrupts
 * 
 * \sa
 */ 
void os_enableIrq (TI_HANDLE OsContext);

/** \brief  OS  IRQ Serviced
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return void
 * 
 * \par Description
 * This function is used in Level IRQ only. At this point the interrupt line is not asserted anymore
 * and we can inform the OS to enable IRQ again.
 * 
 * \sa
 */ 
void os_InterruptServiced (TI_HANDLE OsContext);

/****************************************************************************************
 *						OS Report API													*       
 ****************************************************************************************/

/** \brief  OS Set Debug Mode
 * 
 * \param  enable 	- Indicates if debug mode should be enabled or disabled ( TI_TRUE | TI_FALSE )
 * \return void
 * 
 * \par Description
 * This function sets the Debug Mode flag to True or False - according to user's request
 * 
 * \sa
 */ 
void os_setDebugMode (TI_BOOL enable);

/** \brief  OS Printf
 * 
 * \param  format 	- String to print (with formatted parametrs in string if needed) and parameters values
 *					  if formatted parameters are used in string
 * \return void
 * 
 * \par Description
 * This function prints formatted output using OS available printf method
 * 
 * \sa
 */ 
void os_printf (const char *format ,...);

/****************************************************************************************
 *						OS Memory API													*       
 ****************************************************************************************/

/** \brief  OS Memory Allocation
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  Size 		- Size (in bytes) to be allocated 
 * \return Pointer to the allocated memory on success	;  NULL on failure (there isn't enough memory available)
 * 
 * \par Description
 * This function allocates resident (nonpaged) system-space memory with calling specific OS allocation function.	\n 
 * It is assumed that this function will never be called in an interrupt context since the OS allocation function 
 * has the potential to put the caller to sleep  while waiting for memory to become available.
 * 
 * \sa
 */
void *os_memoryAlloc (TI_HANDLE OsContext,TI_UINT32 Size);

/** \brief  OS Memory CAllocation
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  Number 		- Number of element to be allocated
 * \param  Size 		- Size (in bytes) of one element 
 * \return Pointer to the allocated memory on success	;  NULL on failure (there isn't enough memory available)
 * 
 * \par Description
 * This function allocates an array in memory with elements initialized to 0.
 * Allocates resident (nonpaged) system-space memory for an array with elements initialized to 0, 
 * with specific OS allocation function.
 * It is assumed that this function will never be called in an interrupt context since the OS allocation function 
 * has the potential to put the caller to sleep  while waiting for memory to become available.
 * 
 * \sa
 */
void *os_memoryCAlloc (TI_HANDLE OsContext, TI_UINT32 Number, TI_UINT32 Size);

/** \brief  OS Memory Set
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pMemPtr 		- Pointer to the base address of a memory block 
 * \param  Value 		- Value to set to memory block
 * \param  Length 		- Length (in bytes) of memory block
 * \return void
 * 
 * \par Description
 * This function fills a block of memory with a given value
 * 
 * \sa
 */
void os_memorySet (TI_HANDLE OsContext, void *pMemPtr, TI_INT32 Value, TI_UINT32 Length);

/** \brief  OS Memory Zero
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pMemPtr 		- Pointer to the base address of a memory block 
 * \param  Length 		- Length (in bytes) of memory block
 * \return void
 * 
 * \par Description
 * This function fills a block of memory with zeros
 * 
 * \sa
 */
void os_memoryZero (TI_HANDLE OsContext, void *pMemPtr, TI_UINT32 Length);

/** \brief  OS Memory Copy
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pDestination - Pointer to destination buffer
 * \param  pSource 		- Pointer to Source buffer
 * \param  Size 		- Size (in bytes) to copy
 * \return void
 * 
 * \par Description
 * This function copies a specified number of bytes from one caller-supplied location (source buffer) to another (destination buffer)
 * 
 * \sa
 */
void os_memoryCopy (TI_HANDLE OsContext, void *pDestination, void *pSource, TI_UINT32 Size);

/** \brief  OS Memory Free
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pMemPtr 		- Pointer to the base address of a memory block 
 * \param  Size 		- Size (in bytes) to free
 * \return void
 * 
 * \par Description
 * This function releases a block of memory which was previously allocated by user
 * 
 * \sa
 */
void os_memoryFree (TI_HANDLE OsContext, void *pMemPtr, TI_UINT32 Size);

/** \brief  OS Memory Compare
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  Buf1 		- Pointer to the first buffer in comperation
 * \param  Buf2 		- Pointer to the second buffer in comperation
 * \param  Count 		- Count (in bytes) to compare
 * \return A value which indicates the relationship between the two compared buffers:
 *         value < 0:	Buf1 less than Buf2
 *         value == 0: 	Buf1 identical to Buf2
 *         value > 0: 	Buf1 greater than Buf2
 * 
 * \par Description
 * This function compares between two given buffers
 * 
 * \sa
 */
TI_INT32 os_memoryCompare (TI_HANDLE OsContext, TI_UINT8* Buf1, TI_UINT8* Buf2, TI_INT32 Count);

/** \brief  OS Memory Allocation for HW DMA
 * 
 * \param  pOsContext 	- Handle to the OS object
 * \param  Size 		- Size (in bytes) to allocate
 * \return Pointer to the allocated memory on success	;  NULL on failure (there isn't enough memory available)
 * 
 * \par Description
 * This function allocates resident (nonpaged) system-space memory for HW DMA operations
 * 
 * \sa
 */
void *os_memoryAlloc4HwDma (TI_HANDLE pOsContext, TI_UINT32 Size);

/** \brief  OS Memory for HW DMA Free
 * 
 * \param  pOsContext 	- Handle to the OS object
 * \param  pMem_ptr 	- Pointer to the base virtual address of allocated memory block 
 *                        This is the address that was returned to user when he allocated the memory for HW DMA usage
 * \param  Size 		- Size (in bytes) of the memory block to be released. This parameter must be identical to the Length 
 *						  which was given by the user when he allocated the memory block for HW DMA usage
 * \return Pointer to the allocated memory on success	;  NULL on failure (there isn't enough memory available)
 * 
 * \par Description
 * This function releases a block of memory previously allocated by user for HW DMA operations
 * 
 * \sa
 */
void os_memory4HwDmaFree (TI_HANDLE pOsContext, void *pMem_ptr, TI_UINT32 Size);

/** \brief  OS Memory Copy from User
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pDstPtr 		- Pointer to destination buffer
 * \param  pSrcPtr 		- Pointer to Source buffer
 * \param  Size 		- Size (in bytes) to copy
 * \return TI_OK on success	;  TI_NOK otherwise
 * 
 * \par Description
 * This function copies a specified number of bytes from one caller-supplied location (Source) to another (Destination)
 * 
 * \sa
 */
int os_memoryCopyFromUser (TI_HANDLE OsContext, void *pDstPtr, void *pSrcPtr, TI_UINT32 Size);

/** \brief  OS Memory Copy To User
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pDstPtr 		- Pointer to destination buffer
 * \param  pSrcPtr 		- Pointer to Source buffer
 * \param  Size 		- Size (in bytes) to copy
 * \return TI_OK on success	;  TI_NOK otherwise
 * 
 * \par Description
 * This function copies a specified number of bytes from one caller-supplied location (Source) to another (Destination)
 * 
 * \sa
 */
int os_memoryCopyToUser (TI_HANDLE OsContext, void *pDstPtr, void *pSrcPtr, TI_UINT32 Size);

/****************************************************************************************
 *							OS TIMER API												*
 ****************************************************************************************/
/** \brief  Timer Callback Function
 * 
 * \param  Context 	- Handle to the OS object
 * \return void
 * 
 * \par Description
 * This callback is passed by user to OS timer when created, and is called directly from OS timer context when expired.
 * E.g. the user user the timer in order to operate this function after a defined time expires
 * 
 */
typedef void (*fTimerFunction)(TI_HANDLE Context);

/** \brief  OS Timer Create
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pRoutine 	- Pointer to user's Timer Callback function
 * \param  hFuncHandle 	- Handle to user's Timer Callback function parameters
 * \return Handle to timer object on success	;  NULL on failure
 * 
 * \par Description
 * This function creates and initializes an OS timer object associated with a user's Timer Callback function	\n
 * \note   1) The user's callback is called directly from OS timer context when expired.
 * \note   2) In some OSs, it may be needed to use an intermediate callback in the 
 * \note      osapi layer (use os_timerHandlr for that).
 * 
 * \sa
 */
TI_HANDLE os_timerCreate (TI_HANDLE OsContext, fTimerFunction pRoutine, TI_HANDLE hFuncHandle);

/** \brief  OS Timer Destroy
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  TimerHandle 	- Handle to timer object which user got when created the timer
 * \return void
 * 
 * \par Description
 * This function destroys the OS timer object which was previously created by user
 * 
 * \sa
 */
void os_timerDestroy (TI_HANDLE OsContext, TI_HANDLE TimerHandle);

/** \brief  OS Timer Start
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  TimerHandle 	- Handle to timer object which user got when created the timer
 * \param  DelayMs 		- The time in MS untill the timer is awaken
 * \return void
 * 
 * \par Description
 * This function Start the OS timer object which was previously created by user
 * 
 * \sa
 */
void os_timerStart (TI_HANDLE OsContext, TI_HANDLE TimerHandle, TI_UINT32 DelayMs);

/** \brief  OS Timer Stop
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  TimerHandle 	- Handle to timer object which user got when created the timer
 * \return void
 * 
 * \par Description
 * This function Stops the OS timer object which was previously created by user
 * 
 * \sa
 */
void os_timerStop (TI_HANDLE OsContext, TI_HANDLE TimerHandle);

/** \brief  OS Periodic Interrupt Timer Start
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return void
 * 
 * \par Description
 * This function starts the periodic interrupt mechanism. This function is used when PRIODIC_INTERRUPT mode is used. 
 * This Mode is enabled when interrupts that are usually received from the FW are masked, 
 * and there is need to check- in a given time periods - if handling of any FW interrupt is needed.
 * 
 * \sa
 */
#ifdef PRIODIC_INTERRUPT
void os_periodicIntrTimerStart (TI_HANDLE OsContext);
#endif

/** \brief  OS Time Stamp Ms
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return The number of milliseconds that have elapsed since the system was booted
 * 
 * \par Description
 * This function returns the number of milliseconds that have elapsed since the system was booted.
 */
TI_UINT32 os_timeStampMs (TI_HANDLE OsContext);

/** \brief  OS Time Stamp Us
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return The number of microseconds that have elapsed since the system was booted
 * 
 * \par Description
 * This function returns the number of microseconds that have elapsed since the system was booted.	\n
 * Note that sometimes this function will be called with NULL(!!!) as argument!
 */
TI_UINT32 os_timeStampUs (TI_HANDLE OsContext);

/** \brief  OS Stall uSec
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  uSec 		- The time to delay in microseconds
 * \return void
 * 
 * \par Description
 * This function makes delay in microseconds
 * 
 * \sa
 */
void os_StalluSec (TI_HANDLE OsContext, TI_UINT32 uSec);


/****************************************************************************************
 *							Protection services	API										*
 ****************************************************************************************/

/** \brief  OS Protect Create
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return Handle of the created mutex/spin lock object on Success	; NULL on Failure (not enough memory available or problems to initializing the mutex)
 * 
 * \par Description
 * This function allocates a mutex/spin lock object.
 * The mutex/spinlock object which is created by this function is used for mutual-exclusion and protection of resources which are shared between
 * multi-Tasks/Threads
 * 
 * \sa
 */
TI_HANDLE os_protectCreate (TI_HANDLE OsContext);

/** \brief  OS Protect Destroy
 * 
 * \param  OsContext 		- Handle to the OS object
 * \param  ProtectContext 	- Handle to the mutex/spin lock object
 * \return void
 * 
 * \par Description
 * This function destroys s a mutex/spin lock object which was previously created by user:
 * it frees the mutex/spin lock and then frees the object's memory
 * 
 * \sa
 */
void os_protectDestroy (TI_HANDLE OsContext, TI_HANDLE ProtectContext);

/** \brief  OS Protect Lock
 * 
 * \param  OsContext 		- Handle to the OS object
 * \param  ProtectContext 	- Handle to the mutex/spin lock object
 * \return void
 * 
 * \par Description
 * This function locks the mutex/spin lock object. E.g. the caller acquires a mutex/spin lock and gains exclusive
 * access to the shared resources, that the mutex/spin lock protects of.
 *
 * \sa
 */
void os_protectLock (TI_HANDLE OsContext, TI_HANDLE ProtectContext);

/** \brief  OS Protect Unlock
 * 
 * \param  OsContext 		- Handle to the OS object
 * \param  ProtectContext 	- Handle to the mutex/spin lock object
 * \return void
 * 
 * \par Description
 * This function unlocks the mutex/spin lock object.
 * 
 * \sa
 */
void os_protectUnlock (TI_HANDLE OsContext, TI_HANDLE ProtectContext);

/* Wakelock functionality */
int os_wake_lock (TI_HANDLE OsContext);
int os_wake_unlock (TI_HANDLE OsContext);
int os_wake_lock_timeout (TI_HANDLE OsContext);
int os_wake_lock_timeout_enable (TI_HANDLE OsContext);

#define os_profile(hos,fn,par)


/****************************************************************************************
						START OF GWSI DRIVER API				
*****************************************************************************************/

/** \brief  OS Signaling Object Create
 * 
 * \param  OsContext 		- Handle to the OS object
 * \return Pointer to Signal Object on Success	;	NULL on Failure	
 * 
 * \par Description
 * This function creates a new Signaling Object or opens an already exists Signaling Object.
 * The Signaling Object created by this function is used for mutual-exclusion and protection 
 * of resources which are shared between multi-Tasks/Threads by using a signaling mechanism
 * 
 * \sa
 */
void *os_SignalObjectCreate (TI_HANDLE OsContext);

/** \brief  OS Signaling Object Wait
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  ptr 			- Pointer to Signaling Object previously created by user
 * \return TI_OK (0) on Success	;	TI_NOK (1) on Failure
 * 
 * \par Description
 * This function perform waiting on Signaling Object. The coller waits until signaled or until timeout
 * 
 * \sa
 */
int os_SignalObjectWait (TI_HANDLE OsContext, void *ptr);

/** \brief  OS Signaling Object Set
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  ptr 			- Pointer to Signaling Object previously created by user
 * \return TI_OK (0) on Success	;	TI_NOK (1) on Failure
 * 
 * \par Description
 * This function sets a Signaling Object to signaled state (e.g the siganeling object is released)
 * 
 * \sa
 */
int os_SignalObjectSet (TI_HANDLE OsContext, void *ptr);

/** \brief  OS Signaling Object Free
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  ptr 			- Pointer to Signaling Object previously created by user
 * \return TI_OK (0) on Success	;	TI_NOK (1) on Failure
 * 
 * \par Description
 * This function frees (closes) a Signaling Object Handle
 * 
 * \sa
 */
int os_SignalObjectFree (TI_HANDLE OsContext, void *ptr);

/** \brief  OS Schedule Request
 * 
 * \param  OsContext 	- Handle to the OS object
 * \return TI_OK (0) on Success	;	TI_NOK (1) on Failure
 * 
 * \par Description
 * This function performs scheduling (context switch) according to user request
 * 
 * \sa
 */
int os_RequestSchedule (TI_HANDLE OsContext);


/****************************************************************************************
						START OF TI DRIVER API				
*****************************************************************************************/

/** \brief  OS Read Memory Register UINT32
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  Register		- Pointer to register address
 * \param  Data			- Pointer to output read data
 * \return void
 * 
 * \par Description
 * This function reads register in 32 bit length
 * 
 * \sa
 */
void os_hwReadMemRegisterUINT32 (TI_HANDLE OsContext, TI_UINT32* Register, TI_UINT32* Data);

/** \brief  OS Write Memory Register UINT32
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  Register		- Pointer to register address
 * \param  Data			- Data to write to register
 * \return void
 * 
 * \par Description
 * This function reads register in 32 bit length
 * 
 * \sa
 */
void os_hwWriteMemRegisterUINT32 (TI_HANDLE OsContext, TI_UINT32* Register, TI_UINT32 Data);

/** \brief  OS Receive Packet
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pPacket 		- Pointer to received packet data
 * \param  Length 		- Length of received packet
 * \return TI_TRUE on Success	;	TI_FALSE on Failure
 * 
 * \par Description
 * This function transfers a packet from WLAN driver to OS
 * 
 * \sa
 */
TI_BOOL os_receivePacket(TI_HANDLE OsContext, void *pRxDesc ,void *pPacket, TI_UINT16 Length);

/** \brief  OS Indicate Event
 * 
 * \param  OsContext 	- Handle to the OS object
 * \param  pData 		- Pointer to event data
 * \return TI_OK (0) on Success	;
 * 
 * \par Description
 * This function indicate the OS about different connection driver's events,
 * The function performs the rewuired operations for the event - in the OS side
 * 
 * \sa
 */
TI_INT32 os_IndicateEvent (TI_HANDLE OsContext, IPC_EV_DATA *pData);

/** \brief  OS Send Trace Message to Logger
 * 
 * \param  OsContext    - The OS handle
 * \param  uLevel   	- Severity level of the trace message
 * \param  uFileId  	- Source file ID of the trace message
 * \param  uLineNum 	- Line number of the trace message
 * \param  uParamsNum   - Number of parameters in the trace message
 * \param  ...          - The trace message parameters
 * \return void
 * 
 * \par Description
 * This function sends trace message to logger
 * 
 * \sa
 */
void os_Trace (TI_HANDLE OsContext, TI_UINT32 uLevel, TI_UINT32 uFileId, TI_UINT32 uLineNum, TI_UINT32 uParamsNum, ...);

/** 
 * \fn     os_SetDrvThreadPriority
 * \brief  Called upon init to set WLAN driver thread priority.
 * 
 * \param  OsContext              - The OS handle
 * \param  uWlanDrvThreadPriority - The WLAN driver thread priority
 * \return 
 */
void os_SetDrvThreadPriority (TI_HANDLE OsContext, TI_UINT32 uWlanDrvThreadPriority);


#ifdef __cplusplus
}
#endif

#endif /* __OS_API_H__ */
