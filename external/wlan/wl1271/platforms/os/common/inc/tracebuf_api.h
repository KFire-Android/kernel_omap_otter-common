/*
 * tracebuf_api.h
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

/*
 * Benchmark tracing utility
 */

#ifndef TB_TRACE_API_H

#define TB_TRACE_API_H

#ifdef TI_TRACE_BUF

#define TB_NUM_ENTRIES 256
#define MAX_TB_MSG 128

#define TB_OPTION_NONE      0x00000000
#define TB_OPTION_AUTOPRINT 0x00000001
#define TB_OPTION_PRINTONCE 0x00000002
#define TB_OPTION_STOP      0x80000000

/* Initialization */
int  tb_init(unsigned long options);
int  tb_sprintf(const char *format ,...);
int  tb_trace(int loc, unsigned long p1, unsigned long p2);
void tb_destroy(void);
void tb_scan(void);
void tb_dump(void);
void tb_printf(void);
void tb_set_option(unsigned long option);
void tb_reset_option(unsigned long option);

#define tb_min(x,y) (((x)<(y)) ? (x) : (y)) 

#else  /* #ifdef TI_TRACE_BUF */

#define tb_init(options)
#define tb_sprintf(format ,...)
#define tb_trace(loc, p1, p2)
#define tb_destroy()
#define tb_scan()
#define tb_dump()
#define tb_printf()
#define tb_set_option(option)
#define tb_reset_option(option)

#endif /* #ifdef TI_TRACE_BUF */

#endif
