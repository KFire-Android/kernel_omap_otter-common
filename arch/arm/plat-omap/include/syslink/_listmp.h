/*
 *  _listmp.h
 *
 *  Internal definitions for shared memory doubly linked list.
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */

#ifndef __LISTMP_H_
#define __LISTMP_H_

/* Standard headers */
#include <linux/types.h>
#include <linux/list.h>

#include <listmp.h>
#include <sharedregion.h>

/* Unique module ID. */
#define LISTMP_MODULEID		(0xa413)

/* Created tag */
#define LISTMP_CREATED		0x12181964


/* Structure defining shared memory attributes for the ListMP module. */
struct listmp_attrs {
	u32 status;
	u32 *gatemp_addr;
	struct listmp_elem head;
};

/* Structure defining config parameters for the ListMP module. */
struct listmp_config {
	uint max_runtime_entries;
	/* Maximum number of ListMP's that can be dynamically created and
	added to the NameServer. */
	uint max_name_len; /* Maximum length of name */
};


/* Structure defining processor related information for the ListMP module. */
struct listmp_proc_attrs {
	bool creator; /* Creator or opener */
	u16 proc_id; /* Processor Identifier */
	u32 open_count; /* How many times it is opened on a processor */
};


/* Function to get the configuration */
void listmp_get_config(struct listmp_config *cfg_params);

/* Function to setup the listmp module */
int listmp_setup(const struct listmp_config *config);

/* Function to destroy the listmp module */
int listmp_destroy(void);

#endif /* __LISTMP_H_ */
