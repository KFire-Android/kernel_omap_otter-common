/*
 * tracebuf.c
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

#include "osApi.h"
#include "tracebuf.h"
#include "tracebuf_api.h"
#include "report.h"

typedef struct {
      unsigned long loc;/* trace entry identification */
      unsigned long ts;/* Timestamp */
      unsigned long p1; /* Parameter 1 */
      unsigned long p2; /* Parameter 2 */
      char msg[MAX_TB_MSG];
} tb_entry_t;

typedef struct {
      int pos;
      int count;
      int print_pos;
      int nusers;
      unsigned long self_delay;
      unsigned long options;
      tb_entry_t entry[1]; /* Array of entries */
} tb_control_t;

static tb_control_t *tb_control;

static  int tb_control_size(void)
{
   return TI_FIELD_OFFSET(tb_control_t, entry) + sizeof(tb_entry_t)*TB_NUM_ENTRIES;
}


/* Initialization */
int tb_init(unsigned long options)
{
   if (tb_control)
   {
      ++tb_control->nusers;
      return 0;
   }
   tb_control = (tb_control_t *)TB_MALLOC(tb_control_size());
   if (!tb_control)
      return -1;
   memset(tb_control, 0, tb_control_size());
   tb_control->nusers = 1;

   /* Measure self-delay */
   tb_trace(0, 0, 0);
   tb_trace(0, 0, 0);
   tb_control->self_delay = tb_control->entry[1].ts - tb_control->entry[0].ts;
   tb_control->pos = tb_control->count = 0;
   tb_control->options = options;
   return 0;
}

/* De-initialization */
void tb_destroy(void)
{
   if (--tb_control->nusers)
      return;
   TB_FREE(tb_control );
}

static int tb_next(void)
{
    int pos;
    if (!tb_control || tb_control->print_pos)
       return -1;
    pos = tb_control->pos;
    tb_control->pos = (pos+1) % TB_NUM_ENTRIES;
    ++tb_control->count;

    tb_control->entry[tb_control->pos].ts = 
    tb_control->entry[tb_control->pos].loc= 
    tb_control->entry[tb_control->pos].p1 = 
    tb_control->entry[tb_control->pos].p2 = 0xffffffff;

    return pos;
}
static void tb_autoprint(void)
{
    if ((tb_control->pos == 0) && (tb_control->count))
    {
        if (tb_control->options & TB_OPTION_PRINTONCE)
        {
            tb_printf();
            tb_reset_option(TB_OPTION_PRINTONCE);
        }
        else if (tb_control->options & TB_OPTION_AUTOPRINT)
        {
            tb_printf();
        }
    }
}

/* Add trace entry. not safe, but will do */
int tb_trace(int loc, unsigned long p1, unsigned long p2)
{
   int pos;

   if ((tb_control->options & TB_OPTION_STOP) || ((pos = tb_next()) < 0))
   {
       return -1;
   }
   tb_control->entry[pos].ts = os_timeStampUs(NULL);
   tb_control->entry[pos].loc= loc;
   tb_control->entry[pos].p1 = p1;
   tb_control->entry[pos].p2 = p2;

   return pos;
}
void tb_dump(void)
{
	int j, pos;

	WLAN_OS_REPORT(("Trace Dump:\n"));
	WLAN_OS_REPORT(("===========\n\n"));
    if (tb_control->count < TB_NUM_ENTRIES)
    {
        pos = 0;
    }
    else
    {
        pos = (tb_control->pos + 1) % TB_NUM_ENTRIES;
    }
	for (j=0; (unsigned int)j < tb_min((unsigned int)TB_NUM_ENTRIES,(unsigned int)tb_control->count); j++)
	{
		WLAN_OS_REPORT(("%4i %08x %08x %08x %08x\n", j,
			(int)tb_control->entry[pos].ts,
			(int)tb_control->entry[pos].loc,
			(int)tb_control->entry[pos].p1,
			(int)tb_control->entry[pos].p2));
		pos = (pos+1) % TB_NUM_ENTRIES;
	}

}

int tb_sprintf(const char *format ,...)
{

	va_list ap;
    int pos;

    if ((tb_control->options & TB_OPTION_STOP) || ((pos = tb_next()) < 0))
    {
        return -1;
    }
    tb_control->entry[pos].loc = TB_ID;
	va_start(ap,format);
	vsprintf(&tb_control->entry[pos].msg[0], format, ap);
    tb_autoprint();
    return pos;
}

void tb_printf(void)
{
	int j, pos;
    unsigned long saved_options=tb_control->options;

    tb_set_option(TB_OPTION_STOP);
	WLAN_OS_REPORT(("Trace Dump:\n"));
	WLAN_OS_REPORT(("===========\n\n"));
    if (tb_control->count < TB_NUM_ENTRIES)
    {
        pos = 0;
    }
    else
    {
        pos = (tb_control->pos + 1) % TB_NUM_ENTRIES;
    }
	for (j=0; (unsigned int)j < tb_min((unsigned int)TB_NUM_ENTRIES,(unsigned int)tb_control->count); j++)
	{
		WLAN_OS_REPORT(("%4i id=0x%8x %s \n", j,
tb_control->entry[pos].loc, tb_control->entry[pos].msg));
		pos = (pos+1) % TB_NUM_ENTRIES;
	}
    tb_control->options = saved_options;
}
void tb_set_option(unsigned long option)
{
    tb_control->options |= option;
}

void tb_reset_option(unsigned long option)
{
    tb_control->options &= ~option;
}

void tb_scan(void)
{

  int j,k, Size, nAllocs=0, nFrees=0;
  unsigned long address, Allocs=0, Frees=0;

  for (j=0; j < TB_NUM_ENTRIES; j++)
  {
    Size = (int)tb_control->entry[j].p2;
    if (Size > 0) /* Alloc */
    {
      nAllocs += 1;
      Allocs  += Size;
      address = tb_control->entry[j].p1;
      for (k=j+1; k < TB_NUM_ENTRIES; k++)
      {
	if (address == tb_control->entry[k].p1)
	{
	  if (tb_control->entry[k].p2 != -Size)
	  {
	    TB_PRINTF("Bad free size at 0x%lx address = 0x%lx Size = %ld Allocated = %d\n", 
		   tb_control->entry[k].loc, tb_control->entry[k].p1, (long)tb_control->entry[k].p2, Size);
	  }
	  Frees  += tb_control->entry[k].p2;
	  nFrees += 1;
	  break;
	}
      }
      if (k == TB_NUM_ENTRIES)
      {
	TB_PRINTF("Memory leak at 0x%lx address = 0x%lx Size = %d\n", 
	       tb_control->entry[j].loc, address, Size);
      }
    } 
  }
  TB_PRINTF("tb_scan() Allocs = %ld nAllocs = %d Frees = %ld nFrees = %d\n", Allocs, nAllocs, Frees, nFrees);
}

