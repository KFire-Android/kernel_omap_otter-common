#ifndef _STACK_PROFILE_H_
#define _STACK_PROFILE_H_
unsigned long check_stack(unsigned long *base);
unsigned long check_stack_start(unsigned long *base, unsigned long real_sp,
				int id);
unsigned long check_stack_stop(unsigned long *base, int id);
unsigned long save_stack_context(char *name, int id);
void print_stack(int id);
#endif
