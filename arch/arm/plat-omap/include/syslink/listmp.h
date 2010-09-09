/*
 *  listmp.h
 *
 *  The listmp module defines the shared memory doubly linked list.
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

#ifndef _LISTMP_H_
#define _LISTMP_H_

/* Standard headers */
#include <linux/types.h>

/* Utilities headers */
#include <linux/list.h>
/*#include <heap.h>*/

/* =============================================================================
 *  All success and failure codes for the module
 * =============================================================================
 */
/* The resource is still in use */
#define LISTMP_S_BUSY		2

/* The module has already been setup */
#define LISTMP_S_ALREADYSETUP	1

/* Operation is successful. */
#define LISTMP_SUCCESS		0

/* Generic failure. */
#define LISTMP_E_FAIL		-1

/* Argument passed to a function is invalid. */
#define LISTMP_E_INVALIDARG	-2

/* Memory allocation failed. */
#define LISTMP_E_MEMORY		-3

/* The specified entity already exists. */
#define LISTMP_E_ALREADYEXISTS	-4

/* Unable to find the specified entity. */
#define LISTMP_E_NOTFOUND	-5

/* Operation timed out. */
#define LISTMP_E_TIMEOUT	-6

/* Module is not initialized. */
#define LISTMP_E_INVALIDSTATE	-7

/* A failure occurred in an OS-specific call */
#define LISTMP_E_OSFAILURE	-8

/* Specified resource is not available */
#define LISTMP_E_RESOURCE	-9

/* Operation was interrupted. Please restart the operation */
#define LISTMP_E_RESTART	-10


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define VOLATILE volatile

/* Structure defining list element for the ListMP. */
struct listmp_elem {
	VOLATILE struct listmp_elem *next;
	VOLATILE struct listmp_elem *prev;
};

/* Structure defining config parameters for the ListMP instances. */
struct listmp_params {
	/* gatemp instance for critical management of shared memory */
	void *gatemp_handle;
	void *shared_addr;  /* physical address of the shared memory */
	char *name; /* name of the instance */
	u16 region_id; /* sharedregion id */
};


/* Function initializes listmp parameters */
void listmp_params_init(struct listmp_params *params);

/* Function to create an instance of ListMP */
void *listmp_create(const struct listmp_params *params);

/* Function to delete an instance of ListMP */
int listmp_delete(void **listmp_handle_ptr);

/* Function to open a previously created instance */
int listmp_open(char *name, void **listmp_handle_ptr);

/* Function to open a previously created instance */
int listmp_open_by_addr(void *shared_addr, void **listmp_handle_ptr);

/* Function to close a previously opened instance */
int listmp_close(void **listmp_handle);

/* Function to check if list is empty */
bool listmp_empty(void *listmp_handle);

/* Retrieves the GateMP handle associated with the ListMP instance. */
void *listmp_get_gate(void *listmp_handle);

/* Function to get head element from list */
void *listmp_get_head(void *listmp_handle);

/* Function to get tail element from list */
void *listmp_get_tail(void *listmp_handle);

/* Function to insert element into list */
int listmp_insert(void *listmp_handle, struct listmp_elem *new_elem,
			struct listmp_elem *cur_elem);

/* Function to traverse to next element in list */
void *listmp_next(void *listmp_handle, struct listmp_elem *elem);

/* Function to traverse to prev element in list */
void *listmp_prev(void *listmp_handle, struct listmp_elem *elem);

/* Function to put head element into list */
int listmp_put_head(void *listmp_handle, struct listmp_elem *elem);

/* Function to put tail element into list */
int listmp_put_tail(void *listmp_handle, struct listmp_elem *elem);

/* Function to traverse to remove element from list */
int listmp_remove(void *listmp_handle, struct listmp_elem *elem);

/* Function to get shared memory requirement for the module */
uint listmp_shared_mem_req(const struct listmp_params *params);

#endif /* _LISTMP_H_ */
