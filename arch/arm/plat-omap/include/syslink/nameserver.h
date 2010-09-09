/*
 *  nameserver.h
 *
 *  The nameserver module manages local name/value pairs that
 *  enables an application and other modules to store and retrieve
 *  values based on a name.
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

#ifndef _NAMESERVER_H_
#define _NAMESERVER_H_

#include <linux/types.h>
#include <linux/list.h>

/*
 *  NAMESERVER_MODULEID
 *  Unique module ID
 */
#define NAMESERVER_MODULEID      (0xF414)

struct nameserver_config {
	u32 reserved;
};

/*
 *  Instance config-params object.
 */
struct nameserver_params {
	u32 max_runtime_entries;
	void *table_heap; /* Table is placed into a section on dyn creates */
	bool check_existing; /* Prevents duplicate entry add in to the table */
	u32 max_value_len; /* Length, in MAUs, of the value field */
	u16 max_name_len; /* Length, in MAUs, of name field */
};


/*
 *  Function to get the default configuration for the nameserver module
 */
void nameserver_get_config(struct nameserver_config *cfg);

/*
 *  Function to setup the nameserver module
 */
int nameserver_setup(void);

/*
 *  Function to destroy the nameserver module
 */
int nameserver_destroy(void);

/*
 *  Function to construct a name server.
 */
void nameserver_construct(void *object, const char *name,
				const struct nameserver_params *params);

/*
 *  Function to destruct a name server
 */
void nameserver_destruct(void *object);

/*
 *  Function to register a remote driver
 */
int nameserver_register_remote_driver(void *handle, u16 proc_id);

/*
 *  Function to unregister a remote driver
 */
int nameserver_unregister_remote_driver(u16 proc_id);

/*
 *  Determines if a remote driver is registered for the specified id.
 */
bool nameserver_is_registered(u16 proc_id);

/*
 *  Function to initialize the parameter structure
 */
void nameserver_params_init(struct nameserver_params *params);

/*
 *  Function to create a name server
 */
void *nameserver_create(const char *name,
			const struct nameserver_params *params);

/*
 *  Function to delete a name server
 */
int nameserver_delete(void **handle);

/*
 *  Function to handle for  a name
 */
void *nameserver_get_handle(const char *name);

/*
 *  Function to add a variable length value into the local table
 */
void *nameserver_add(void *handle, const char *name, void *buf, u32 len);

/*
 *  Function to add a 32 bit value into the local table
 */
void *nameserver_add_uint32(void *handle, const char *name, u32 value);

/*
 *  Function to retrieve the value portion of a name/value pair
 */
int nameserver_get(void *handle, const char *name, void *buf, u32 *len,
			u16 procId[]);

/*
 *  Function to retrieve a 32-bit value of a name/value pair
 */
int nameserver_get_uint32(void *handle, const char *name, void *buf,
				u16 procId[]);

/*
 *  Function to get the value portion of a name/value pair from local table
 */
int nameserver_get_local(void *handle, const char *name, void *buf, u32 *len);

/*
 *  Function to retrieve a 32-bit value from the local name/value table
 */
int nameserver_get_local_uint32(void *handle, const char *name, void *buf);

/*
 *  Function to match the name
 */
int nameserver_match(void *handle, const char *name, u32 *value);

/*
 *  Function to removes a value/pair
 */
int nameserver_remove(void *handle, const char *name);

/*
 *  Function to remove an entry from the table
 */
int nameserver_remove_entry(void *handle, void *entry);

#endif /* _NAMESERVER_H_ */
