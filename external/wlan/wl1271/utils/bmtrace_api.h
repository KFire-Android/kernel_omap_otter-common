/*
 * bmtrace_api.h
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


/** \file   bmtrace_api.h 
 *  \brief  bmtrace performance tracing module API definition                                  
 *
 *  \see    bmtrace.c
 */

#ifndef __BM_TRACE_API_H
#define __BM_TRACE_API_H

void            bm_enable(void);
void            bm_disable(void);
unsigned long   bm_act_trace_in(void);
void            bm_act_trace_out(int loc, unsigned long in_ts);
int             bm_act_register_event(char* module, char* context, char* group, unsigned char level, char* name, char* suffix, int is_param);
void            bm_init(void);
int             print_out_buffer(char *buf);


#ifdef TIWLAN_BMTRACE
#define TIWLAN_CLT_LEVEL 1
#else
#define TIWLAN_CLT_LEVEL 0
#endif

#define CL_TRACE_START() \
	unsigned long in_ts = bm_act_trace_in();

#define CL_TRACE_RESTART_Lx() \
	in_ts = bm_act_trace_in();

#define CL_TRACE_END(MODULE, CONTEXT, GROUP, LEVEL, SUFFIX) \
	{ \
		static int loc = 0; \
		if (loc==0) \
			loc = bm_act_register_event(MODULE, CONTEXT, GROUP, LEVEL, (char*)__FUNCTION__, SUFFIX, 0); \
		bm_act_trace_out(loc, in_ts); \
	}

	#define CL_TRACE_START_L0() CL_TRACE_START()
	#define CL_TRACE_END_L0() CL_TRACE_END("KERNEL", "SYS_CALL", "PLATFORM_TEST", 0, "")

#if TIWLAN_CLT_LEVEL == 1
    #define CL_TRACE_INIT()     bm_init()
    #define CL_TRACE_ENABLE()   bm_enable()
    #define CL_TRACE_DISABLE()  bm_disable()
    #define CL_TRACE_PRINT(buf) print_out_buffer(buf)
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L2() 
	#define CL_TRACE_START_L3() 
	#define CL_TRACE_START_L4() 
	#define CL_TRACE_START_L5() 
    #define CL_TRACE_RESTART()  CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 1, SUFFIX)
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) 
#elif TIWLAN_CLT_LEVEL == 2
    #define CL_TRACE_INIT()     bm_init()
    #define CL_TRACE_ENABLE()   bm_enable()
    #define CL_TRACE_DISABLE()  bm_disable()
    #define CL_TRACE_PRINT(buf) print_out_buffer(buf)
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L2() CL_TRACE_START()
	#define CL_TRACE_START_L3() 
	#define CL_TRACE_START_L4() 
	#define CL_TRACE_START_L5() 
    #define CL_TRACE_RESTART()  CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 1, SUFFIX)
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 2, SUFFIX)
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) 
#elif TIWLAN_CLT_LEVEL == 3
    #define CL_TRACE_INIT()     bm_init()
    #define CL_TRACE_ENABLE()   bm_enable()
    #define CL_TRACE_DISABLE()  bm_disable()
    #define CL_TRACE_PRINT(buf) print_out_buffer(buf)
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L2() CL_TRACE_START()
	#define CL_TRACE_START_L3() CL_TRACE_START()
	#define CL_TRACE_START_L4() 
	#define CL_TRACE_START_L5() 
    #define CL_TRACE_RESTART()  CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 1, SUFFIX)
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 2, SUFFIX)
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 3, SUFFIX)
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) 
#elif TIWLAN_CLT_LEVEL == 4
    #define CL_TRACE_INIT()     bm_init()
    #define CL_TRACE_ENABLE()   bm_enable()
    #define CL_TRACE_DISABLE()  bm_disable()
    #define CL_TRACE_PRINT(buf) print_out_buffer(buf)
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L2() CL_TRACE_START()
	#define CL_TRACE_START_L3() CL_TRACE_START()
	#define CL_TRACE_START_L4() CL_TRACE_START()
	#define CL_TRACE_START_L5() 
    #define CL_TRACE_RESTART()  CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 1, SUFFIX)
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 2, SUFFIX)
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 3, SUFFIX)
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 4, SUFFIX)
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) 
#elif TIWLAN_CLT_LEVEL == 5
    #define CL_TRACE_INIT()     bm_init()
    #define CL_TRACE_ENABLE()   bm_enable()
    #define CL_TRACE_DISABLE()  bm_disable()
    #define CL_TRACE_PRINT(buf) print_out_buffer(buf)
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L1() CL_TRACE_START()
	#define CL_TRACE_START_L2() CL_TRACE_START()
	#define CL_TRACE_START_L3() CL_TRACE_START()
	#define CL_TRACE_START_L4() CL_TRACE_START()
	#define CL_TRACE_START_L5() CL_TRACE_START()
    #define CL_TRACE_RESTART()  CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 1, SUFFIX)
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 2, SUFFIX)
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 3, SUFFIX)
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 4, SUFFIX)
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) CL_TRACE_END(MODULE, CONTEXT, GROUP, 5, SUFFIX)
#else
    #define CL_TRACE_INIT()   
    #define CL_TRACE_ENABLE() 
    #define CL_TRACE_DISABLE()
    #define CL_TRACE_RESTART() 
    #define CL_TRACE_PRINT(buf)  
	#define CL_TRACE_START_L1() 
	#define CL_TRACE_START_L1() 
	#define CL_TRACE_START_L2() 
	#define CL_TRACE_START_L3() 
	#define CL_TRACE_START_L4() 
	#define CL_TRACE_START_L5() 
	#define CL_TRACE_END_L1(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L2(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L3(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L4(MODULE, CONTEXT, GROUP, SUFFIX) 
	#define CL_TRACE_END_L5(MODULE, CONTEXT, GROUP, SUFFIX) 
#endif


#endif /* __BM_TRACE_API_H */
