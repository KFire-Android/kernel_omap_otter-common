/*
 * public_descriptors.h
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

/**********************************************************************************************************************

  FILENAME:       public_descriptors.h

  DESCRIPTION:    Contains the host interface descriptor types in use.



***********************************************************************************************************************/
#ifndef PUBLIC_DESCRIPTORS_H
#define PUBLIC_DESCRIPTORS_H


#include "public_types.h"



/******************************************************************************

		TX PATH
	 
******************************************************************************/

#define AID_BROADCAST 0x0       /* broadcast frames AID */
#define AID_GLOBAL    0xFF      /* unassociated STAs AID */

#define TRQ_DEPTH           16      /* depth of TX Result Queue */

#define NUM_TX_DESCRIPTORS  32      /* Total number of Tx descriptors in the FW */

/* TX attributes masks and offset used in the txAttr of TxIfDescriptor_t. */
#define     TX_ATTR_SAVE_RETRIES          BIT_0
#define     TX_ATTR_HEADER_PAD            BIT_1
#define     TX_ATTR_SESSION_COUNTER       (BIT_2 | BIT_3 | BIT_4)
#define     TX_ATTR_RATE_POLICY           (BIT_5 | BIT_6 | BIT_7 | BIT_8 | BIT_9)
#define     TX_ATTR_LAST_WORD_PAD         (BIT_10 | BIT_11)
#define     TX_ATTR_TX_CMPLT_REQ          BIT_12

#define     TX_ATTR_OFST_SAVE_RETRIES     0
#define     TX_ATTR_OFST_HEADER_PAD       1
#define     TX_ATTR_OFST_SESSION_COUNTER  2
#define     TX_ATTR_OFST_RATE_POLICY      5
#define     TX_ATTR_OFST_LAST_WORD_PAD    10
#define     TX_ATTR_OFST_TX_CMPLT_REQ     12

/* The packet transmission result, written in the status field of TxResultDescriptor_t */
typedef enum
{
    TX_SUCCESS              = 0,     
	TX_HW_ERROR             = 1,
	TX_DISABLED             = 2,
	TX_RETRY_EXCEEDED       = 3,
	TX_TIMEOUT              = 4,
	TX_KEY_NOT_FOUND        = 5,
	TX_PEER_NOT_FOUND       = 6,
    TX_SESSION_MISMATCH     = 7 
} TxDescStatus_enum;

#ifdef HOST_COMPILE
typedef uint8 TxDescStatus_e;
#else
typedef TxDescStatus_enum TxDescStatus_e;
#endif

/* The Tx Descriptor preceding each Tx packet copied to the FW (before the packet). */
typedef struct TxIfDescriptor_t
{
    uint16          length;		/* Length of packet in words, including descriptor+header+data */
    uint8           extraMemBlks; /* Number of extra memory blocks to allocate for this packet in addition 
                                       to the number of blocks derived from the packet length */
    uint8           totalMemBlks;   /* Total number of memory blocks allocated by the host for this packet. 
                                    Must be equal or greater than the actual blocks number allocated by HW!! */
    uint32          startTime;  /* Device time (in us) when the packet arrived to the driver */
    uint16          lifeTime;   /* Max delay in TUs until transmission. The last device time the 
                                      packet can be transmitted is: startTime+(1024*LifeTime) */ 
    uint16          txAttr;		/* Bitwise fields - see TX_ATTR... definitions above. */
    uint8           descID;		/* Packet identifier used also in the Tx-Result. */
    uint8           tid;		/* The packet TID value (as User-Priority) */
    uint8           aid;	    /* Identifier of the remote STA in IBSS, 1 in infra-BSS */
    uint8           reserved;       /* For HW use, set to 0 */

} TxIfDescriptor_t;


/* The Tx result retrieved from FW upon TX completion. */
typedef struct TxResultDescriptor_t
{
    uint8			descID;		 /* Packet Identifier - same value used in the Tx descriptor.*/
	TxDescStatus_e	status;		 /* The status of the transmission, indicating success or one of several
									 possible reasons for failure. Refer to TxDescStatus_enum above.*/
    uint16 			mediumUsage; /* Total air access duration including all retrys and overheads.*/
    uint32 			fwHandlingTime;	/* The time passed from host xfer to Tx-complete.*/
    uint32 			mediumDelay; /* Total media delay (from 1st EDCA AIFS counter until TX Complete). */
    uint8  			lsbSecuritySequenceNumber; /* LS-byte of last TKIP seq-num (saved per AC for recovery).*/
    uint8  			ackFailures; /* Retry count - number of transmissions without successful ACK.*/
    TxRateIndex_t	rate;		 /* The rate that succeeded getting ACK (Valid only if status=SUCCESS). */
    uint8  			spare;       /* for 4-byte alignment. */  
} TxResultDescriptor_t;

/* The Host-FW Tx-Result control counters */
typedef struct
{
	uint32 TxResultFwCounter;	/* FW updates num of results written to results-queue since FW-init. */
    uint32 TxResultHostCounter;	/* Host updates num of results read from results-queue since FW-init. */
} TxResultControl_t;

/* The Host-FW Tx-Result Interface */
typedef struct 
{
	TxResultControl_t TxResultControl;  		   /* See above. */
	TxResultDescriptor_t TxResultQueue[TRQ_DEPTH];
} TxResultInterface_t;


/******************************************************************************

		RX PATH

******************************************************************************/
/* ------------------------------------- */
/* flags field in the RxIfDescriptor_t   */
/* ------------------------------------- */
/*   Bit5-7: Encryption type:            */
/*           0 - none                    */
/*           1 - WEP                     */
/*           2 - TKIP                    */
/*           3 - AES                     */
/*           4 - GEM                     */
/*   Bit4: HT                            */
/*   Bit3: Was part of A-MPDU            */
/*   Bit2: STBC                          */
/*   Bit0-1: Band the frame was received */
/*           from (0=2.4, 1=4.9, 2=5.0)  */
/* ------------------------------------- */
#define    RX_DESC_BAND_MASK        0x03  /* Band is in Bits 0-1 */
#define    RX_DESC_BAND_BG          0x00  
#define    RX_DESC_BAND_J           0x01  
#define    RX_DESC_BAND_A           0x02
#define    RX_DESC_STBC             0x04
#define    RX_DESC_A_MPDU           0x08
#define    RX_DESC_HT               0x10
#define    RX_DESC_ENCRYPT_MASK     0xE0  /* Encryption is in Bits 5-7 */
#define    RX_DESC_ENCRYPT_WEP      0x20
#define    RX_DESC_ENCRYPT_TKIP     0x40
#define    RX_DESC_ENCRYPT_AES      0x60
#define    RX_DESC_ENCRYPT_GEM      0x80


/* ------------------------------------- */
/* Status field in the RxIfDescriptor_t  */
/* ------------------------------------- */
/*   Bit3-7: reserved (0)                */
/*   Bit0-2: 0 - Success,                */
/*           1 - RX_DECRYPT_FAIL,        */
/*           2 - RX_MIC_FAIL             */ 
/* ------------------------------------- */
#define    RX_DESC_STATUS_SUCCESS           0  
#define    RX_DESC_STATUS_DECRYPT_FAIL      1  
#define    RX_DESC_STATUS_MIC_FAIL          2
#define    RX_DESC_STATUS_DRIVER_RX_Q_FAIL  3

#define    RX_DESC_STATUS_MASK              7


/**********************************************
    clasify tagging 
***********************************************/
typedef enum
{
    TAG_CLASS_UNKNOWN       = 0,
    TAG_CLASS_MANAGEMENT    = 1, /* other than Beacon or Probe Resp */
    TAG_CLASS_DATA          = 2,
    TAG_CLASS_QOS_DATA      = 3,
    TAG_CLASS_BCN_PRBRSP    = 4,
    TAG_CLASS_EAPOL         = 5, 
    TAG_CLASS_BA_EVENT      = 6,
    TAG_CLASS_AMSDU         = 7
} PacketClassTag_enum;

#ifdef HOST_COMPILE
typedef uint8 PacketClassTag_e;
#else
typedef PacketClassTag_enum PacketClassTag_e;
#endif

typedef uint8 ProcessIDTag_e;


/* ------------------------------------------------------- */
/* flags field in the driverFlags of the RxIfDescriptor_t  */
/* ------------------------------------------------------- */
/*   Bit0   :  EndOfBurst flag                              */
/*   Bit1-7 : - not in use                                 */
/* ------------------------------------------------------- */

#define DRV_RX_FLAG_END_OF_BURST  0x01



/******************************************************************************

    RxIfDescriptor_t

    the structure of the Rx Descriptor recieved by HOST.

******************************************************************************/
typedef struct
{    
	uint16              length;             /* Length of payload (including headers)*/
    
    uint8               status;             /* 0 = Success, 1 = RX Decrypt Fail, 2 = RX MIC Fail */

    uint8               flags;              /* See RX_DESC_xxx above */

    TxRateIndex_t       rate;               /* Recevied Rate:at ETxRateClassId format */

    uint8               channel;            /* The received channel*/
    
    int8                rx_level;           /* The computed RSSI value in db of current frame */  
    
    uint8               rx_snr;             /* The computed SNR value in db of current frame */
                                            
    uint32              timestamp;          /* Timestamp in microseconds,     */

    PacketClassTag_e    packet_class_tag;   /* Packet classification tagging info */

    ProcessIDTag_e      proccess_id_tag;    /* Driver defined ID */

    uint8               extraBytes;         /* Number of padding bytes added to actual packet length */

    uint8               driverFlags;        /* holds the driver flags to be used internally */
    
} RxIfDescriptor_t;



#endif /* PUBLIC_DESCRIPTORS_H*/



