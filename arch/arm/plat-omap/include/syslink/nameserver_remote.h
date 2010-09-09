/*
 *  nameserver_remote.h
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

#ifndef _NAMESERVER_REMOTE_H_
#define _NAMESERVER_REMOTE_H_

#include <linux/types.h>

/*
 *  Structure defining object for the nameserver remote driver
 */
struct nameserver_remote_object {
	int (*get)(const struct nameserver_remote_object *obj,
		const char *instance_name, const char *name,
		void *value, u32 *value_len, void *reserved);
		/* Function to get data from remote nameserver */
	void *obj; /* Implementation specific object */
};

/*
 *  Function get data from remote name server
 */
int nameserver_remote_get(const struct nameserver_remote_object *handle,
			const char *instance_name, const char *name,
			void *value, u32 *value_len, void *reserved);

#endif /* _NAMESERVER_REMOTE_H_ */
