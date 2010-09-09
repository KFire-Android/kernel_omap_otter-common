/*
* multiproc.h
*
* Many multi-processor modules have the concept of processor id. multiproc
* centeralizes the processor id management.
*
* Copyright (C) 2008-2009 Texas Instruments, Inc.
*
* This package is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
* WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
* PURPOSE.
*/

#ifndef _MULTIPROC_H_
#define _MULTIPROC_H_

#include <linux/types.h>


#define VOLATILE volatile

/*
 *  Unique module ID
 */
#define MULTIPROC_MODULEID		(u16)0xB522

/* Macro to define invalid processor id */
#define MULTIPROC_INVALIDID		((u16)0xFFFF)

/*
 *  Maximum number of processors in the system
 *  OMAP4 has 4 processors in single core.
 */
#define MULTIPROC_MAXPROCESSORS 4

/*
 *  Max name length for a processor name
 */
#define MULTIPROC_MAXNAMELENGTH 32

/*
 *  Configuration structure for multiproc module
 */
struct multiproc_config {
	s32 num_processors; /* Number of procs for particular system */
	char name_list[MULTIPROC_MAXPROCESSORS][MULTIPROC_MAXNAMELENGTH];
	/* Name List for processors in the system */
	u16 id; /* Local Proc ID. This needs to be set before calling any
			other APIs */
};

/* =============================================================================
 *  APIs
 * =============================================================================
 */

/* Function to get the default configuration for the multiproc module. */
void multiproc_get_config(struct multiproc_config *cfg);

/* Function to setup the multiproc Module */
s32 multiproc_setup(struct multiproc_config *cfg);

/* Function to destroy the multiproc module */
s32 multiproc_destroy(void);

/* Function to set local processor Id */
int multiproc_set_local_id(u16 proc_id);

/* Function to get processor id from processor name. */
u16  multiproc_get_id(const char *proc_name);

/* Function to get name from processor id. */
char *multiproc_get_name(u16 proc_id);

/* Function to get number of processors in the system. */
u16 multiproc_get_num_processors(void);

/* Return Id of current processor */
u16 multiproc_self(void);

/* Determines the offset for any two processors. */
u32 multiproc_get_slot(u16 remote_proc_id);

#endif	/* _MULTIPROC_H_ */
