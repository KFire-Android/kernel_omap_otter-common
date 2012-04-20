/*
 * stack_profile.c
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
 * src/stack_profile.c
 *
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#define STACK_MASK		(THREAD_SIZE-1)
#define MAX_STACK_FRAME		2

typedef struct STACK_FRAME {
	unsigned long stack_buf[THREAD_SIZE/sizeof(unsigned long)];
	unsigned long *stack_start;
	unsigned long stack_size;
} stack_frame_t;

static stack_frame_t sf_array[MAX_STACK_FRAME];

static unsigned long check_stack(unsigned long *base)
{
	register unsigned long sp asm ("sp");
	unsigned long retval = sp;

	*base = ((sp & ~STACK_MASK) + sizeof(struct task_struct) + 4);
	return retval;
}

unsigned long check_stack_start(unsigned long *base, unsigned long real_sp,
				int id)
{
	unsigned long i;
	unsigned long from, to;

	to = check_stack(&from);
	*base = from;

	/* save used stack context */
	if (id < MAX_STACK_FRAME) {
		stack_frame_t *sfp = &sf_array[id];

		if (!real_sp)
			real_sp = to;
		sfp->stack_size = THREAD_SIZE - (real_sp & STACK_MASK);
		sfp->stack_start = (unsigned long *)real_sp;
		memcpy(sfp->stack_buf, sfp->stack_start, sfp->stack_size);
	}
	/* run from the stack pointer down to the base */
	for(i=from;(i < to);i+=4) {
		/* fill up the pattern */
		*(unsigned long *)i = 0xdeadbeef;
	}
	/*printk("check_stack_start: from=%x to=%x data=%x\n",from,to,*(long *)(from+4));*/
	return to;
}

unsigned long check_stack_stop(unsigned long *base, int id)
{
	unsigned long i;
	unsigned long from, to;

	to = check_stack(&from);
	*base = from;

	/* check used stack context */
	if (id < MAX_STACK_FRAME) {
		stack_frame_t *sfp = &sf_array[id];

		if (memcmp(sfp->stack_buf, sfp->stack_start, sfp->stack_size)) {
			printk("%s: %p - Error\n", __func__, sfp->stack_start);
			for(i=0;(i < sfp->stack_size/sizeof(unsigned long));i++) {
				if (sfp->stack_start[i] != sfp->stack_buf[i])
					printk("%p: 0x%08lx != 0x%08lx\n", &sfp->stack_start[i], sfp->stack_start[i], sfp->stack_buf[i]);
			}				
		}
	}

	/* run from the stack pointer down to the base */
	for(i=from;(i < to);i+=4) {
		/* check up the pattern */
		if ((*(unsigned long *)i) != 0xdeadbeef)
			break;
	}

	/*printk("check_stack_stop: from=%x to=%x data=%x data=%x i=0x%x\n",from,to,*(long *)from,*(long *)(from+4),i);*/
	/* return the first time when the pattern doesn't match */
	return i;
}

void print_stack(int id)
{
	stack_frame_t *sfp = &sf_array[id];
	unsigned long i;

	printk("%s: %d\n", __func__, id);
	for(i=0;(i < sfp->stack_size/sizeof(unsigned long));i++) {
		printk("%p: 0x%08lx\n", &sfp->stack_start[i], sfp->stack_start[i]);
	}				
}

struct task_struct *get_task_struct_ptr_by_name(char *name)
{
	struct task_struct *g, *p;

	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		/*
		 * reset the NMI-timeout, listing all files on a slow
		 * console might take alot of time:
		 */
		/* touch_nmi_watchdog(); */
		if (!strcmp(name, p->comm)) {
			read_unlock(&tasklist_lock);
			return p;
		}
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
	return NULL;
}

unsigned long save_stack_context(char *name, int id)
{
/*	register unsigned long sp asm ("sp");
	unsigned long sp_local = sp;
*/
	struct task_struct *p;
	stack_frame_t *sfp;

	if (id >= MAX_STACK_FRAME)
		return 0L;
	sfp = &sf_array[id];		
	p = get_task_struct_ptr_by_name(name);
	if (p) {
		printk("%s: %s found\n", __func__, p->comm); /* sched_show_task(t);*/
		sfp->stack_start = (unsigned long *)((unsigned long)end_of_stack(p) & ~STACK_MASK);
		sfp->stack_size = THREAD_SIZE;
		memcpy(sfp->stack_buf, sfp->stack_start, sfp->stack_size);
	}
	return sfp->stack_size;
}
