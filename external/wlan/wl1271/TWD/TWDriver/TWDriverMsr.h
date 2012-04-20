/*
 * TWDriverMsr.h
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

#ifndef TWDDRIVERMSR_H
#define TWDDRIVERMSR_H

/** \file  TWDriverMsr.h 
 *  \brief TWDriver Measurement APIs
 *
 *  \see 
 */

#include "TWDriverScan.h"
#include "tidef.h"
#include "public_radio.h"

#define NOISE_HISTOGRAM_LENGTH              8
#define MAX_NUM_OF_MSR_TYPES_IN_PARALLEL    3

/* The size of the time frame in which we must start the */
/* measurement request or give up */
#define MSR_START_MAX_DELAY             50

/* In non unicast measurement requests a random delay */
/* between 4 and 40 milliseconds */
#define MSR_ACTIVATION_DELAY_RANDOM     36
#define MSR_ACTIVATION_DELAY_OFFSET     4


/** \enum EMeasurementType
 * \brief different measurement types
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum 
{
/*	0	*/	MSR_TYPE_BASIC_MEASUREMENT  = 0,			/**< */
/*	1	*/	MSR_TYPE_CCA_LOAD_MEASUREMENT,				/**< */
/*	2	*/	MSR_TYPE_NOISE_HISTOGRAM_MEASUREMENT,		/**< */
/*	3	*/	MSR_TYPE_BEACON_MEASUREMENT,				/**< */
/*	4	*/	MSR_TYPE_FRAME_MEASUREMENT,					/**< */
/*	5	*/	MSR_TYPE_MAX_NUM_OF_MEASURE_TYPES			/**< */

} EMeasurementType;

/** \enum EMeasurementScanMode
 * \brief Measurement Scan Modes
 * 
 * \par Description
 * enumerates the different scan modes available for beacon measurement
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	MSR_SCAN_MODE_PASSIVE = 0,				/**< Passive Scan Mode			*/
/*	1	*/	MSR_SCAN_MODE_ACTIVE,					/**< Active Scan Mode			*/
/*	2	*/	MSR_SCAN_MODE_BEACON_TABLE,				/**< Beacon Table Scan Mode		*/
/*	3	*/	MSR_SCAN_MODE_MAX_NUM_OF_SCAN_MODES		/**< Max number of Scan Modes	*/

} EMeasurementScanMode;

/** \enum EMeasurementFrameType
 * \brief Measurement Frame Types
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum 
{
/*	0	*/	MSR_FRAME_TYPE_NO_ACTIVE = 0,	/**< */
/*	1	*/	MSR_FRAME_TYPE_BROADCAST,		/**< */
/*	2	*/	MSR_FRAME_TYPE_MULTICAST,		/**< */
/*	3	*/	MSR_FRAME_TYPE_UNICAST			/**< */

} EMeasurementFrameType;

/** \enum EMeasurementMode
 * \brief Measurement Modes
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum
{
/*	0	*/	MSR_MODE_NONE = 0,				/**< */
/*	1	*/	MSR_MODE_XCC,					/**< */
/*	2	*/	MSR_MODE_SPECTRUM_MANAGEMENT	/**< */

} EMeasurementMode;

/** \enum EMeasurementRejectReason
 * \brief Measurement Reject Reason
 * 
 * \par Description
 * 
 * \sa
 */
typedef enum 
{
/*	1	*/	MSR_REJECT_OTHER_REASON = 1, 				/**< */
/*	2	*/	MSR_REJECT_INVALID_MEASUREMENT_TYPE,		/**< */		
/*	3	*/	MSR_REJECT_DTIM_OVERLAP,					/**< */
/*	4	*/	MSR_REJECT_DURATION_EXCEED_MAX_DURATION,	/**< */
/*	5	*/	MSR_REJECT_TRAFFIC_INTENSITY_TOO_HIGH,		/**< */
/*	6	*/	MSR_REJECT_SCR_UNAVAILABLE,					/**< */
/*	7	*/	MSR_REJECT_MAX_DELAY_PASSED,				/**< */
/*	8	*/	MSR_REJECT_INVALID_CHANNEL,					/**< */
/*	9	*/	MSR_REJECT_NOISE_HIST_FAIL,					/**< */
/*	10	*/	MSR_REJECT_CHANNEL_LOAD_FAIL,				/**< */
/*	11	*/	MSR_REJECT_EMPTY_REPORT						/**< */

} EMeasurementRejectReason;

 /*
 ***********************************************************************
 *  Unions.
 ***********************************************************************
 */
/** \union TMeasurementReplyValue
 * \brief Measurement possible Reply Values 
 * 
 * \par Description
 * 
 * \sa
 */
typedef union
{
    TI_UINT8                            CCABusyFraction;						/**< */
    TI_UINT8                            RPIDensity[ NOISE_HISTOGRAM_LENGTH ];   /**< */

} TMeasurementReplyValue;

/***********************************************************************
 *  Structure definitions.
 ***********************************************************************
 */
/** \struct TMeasurementTypeRequest
 * \brief Measurement Type Request 
 * 
 * \par Description
 * This structure defines single channel parameters for normal scan operation (inc. triggered)
 * 
 * \sa
 */
typedef struct
{
    EMeasurementType                    msrType;	/**< */
    EMeasurementScanMode                scanMode;	/**< */
    TI_UINT32                           duration;	/**< */
    TI_UINT8                            reserved;	/**< */

} TMeasurementTypeRequest;

/** \struct TMeasurementRequest
 * \brief Measurement Request 
 * 
 * \par Description
 * This structure defines measurement parameters of several measurement request types
 * for one channel
 * 
 * \sa
 */
typedef struct
{
    ERadioBand                          band;												/**< */
    TI_UINT8                            channel;											/**< */
    TI_UINT64                           startTime;											/**< */
    TI_UINT8                            txPowerDbm;								  			/**< */  			
    EScanResultTag                      eTag;												/**< */
    TI_UINT8                            numberOfTypes;										/**< */
    TMeasurementTypeRequest             msrTypes[ MAX_NUM_OF_MSR_TYPES_IN_PARALLEL ];		/**< */

} TMeasurementRequest;

/** \struct TMeasurementTypeReply
 * \brief Measurement Type Reply 
 * 
 * \par Description
 * This structure defines the reply parameters for measurement of specific type performed
 * (the type is indicated in the msrType field)
 * 
 * \sa
 */
typedef struct
{
    EMeasurementType                    msrType;		/**< The type of performed measurement the reply reffer to	*/
    TI_UINT8                            status;			/**< The status of measurement performed					*/
    TMeasurementReplyValue              replyValue;		/**< The Reply Value of performed measurement			 	*/
    TI_UINT8                            reserved;		/**< */

} TMeasurementTypeReply;

/** \struct TMeasurementReply
 * \brief Measurement Reply 
 * 
 * \par Description
 * This structure defines the reply parameters for some measurements of some types performed
 * 
 * \sa
 */
typedef struct 
{
    TI_UINT8                            numberOfTypes;									/**< Number of measurements types (equal to number of measurement replys)	*/
    TMeasurementTypeReply               msrTypes[ MAX_NUM_OF_MSR_TYPES_IN_PARALLEL ];	/**< Measurements Replys buffer. One Reply per type							*/

} TMeasurementReply;

/** \struct TMeasurementFrameHdr
 * \brief Measurement Frame Header 
 * 
 * \par Description
 * This structure defines a Header of a measurement
 * 
 * \sa
 */
typedef struct 
{
    TI_UINT16                           dialogToken;			/**< Indicates if the received Measurement is the same as the one that is being processed	*/
    TI_UINT8                            activatioDelay;			/**< */
    TI_UINT8                            measurementOffset;		/**< */

} TMeasurementFrameHdr;

/** \struct TMeasurementFrameRequest
 * \brief Measurement Frame Request 
 * 
 * \par Description
 * 
 * \sa
 */
typedef struct 
{
    TMeasurementFrameHdr                 *hdr; 			/**< */
    EMeasurementFrameType                frameType;		/**< */
    TI_UINT8                             *requests;		/**< */
    TI_INT32                             requestsLen;	/**< */

} TMeasurementFrameRequest;


#endif /* #define TWDDRIVERMSR_H */
