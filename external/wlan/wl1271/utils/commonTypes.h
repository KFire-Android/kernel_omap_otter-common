/*
 * commonTypes.h
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

#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

/************************************/
/*      System return values.       */
/************************************/

typedef enum
{
    PARAM_NOT_SUPPORTED         = 2,
    PARAM_VALUE_NOT_VALID       = 3,
    CONFIGURATION_NOT_VALID     = 4,
    NO_SITE_SELECTED_YET        = 5,
    EXTERNAL_SET_PARAM_DENIED   = 7,
    EXTERNAL_GET_PARAM_DENIED   = 8,
    PARAM_MODULE_NUMBER_INVALID = 9,
    STATION_IS_NOT_RUNNING      = 10,
    CARD_IS_NOT_INSTALLED       = 11,

    /* QoSMngr */
    NOT_CONNECTED,
    TRAFIC_ADM_PENDING,
    NO_QOS_AP,
    ADM_CTRL_DISABLE,
    AC_ALREADY_IN_USE,
    USER_PRIORITY_NOT_ADMITTED,


	COMMAND_PENDING,

    /* Rx Data Filters */
    RX_NO_AVAILABLE_FILTERS,
    RX_FILTER_ALREADY_EXISTS,
    RX_FILTER_DOES_NOT_EXIST,
    /* Soft Gemini */
    SG_REJECT_MEAS_SG_ACTIVE,
    PARAM_ALREADY_CONFIGURED

} systemStatus_e;

#endif /* __COMMON_TYPES_H__ */

