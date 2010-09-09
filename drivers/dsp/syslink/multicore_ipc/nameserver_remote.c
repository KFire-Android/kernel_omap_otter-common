/*
 *  nameserver_remote.c
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

#include <linux/types.h>
#include <linux/slab.h>

#include <nameserver_remote.h>

/* This will get data from remote name server */
int nameserver_remote_get(const struct nameserver_remote_object *handle,
				const char *instance_name, const char *name,
				void *value, u32 *value_len, void *reserved)
{
	s32 retval = 0;

	if (WARN_ON(unlikely(handle == NULL))) {
		retval = -EINVAL;
		goto exit;
	}

	if (WARN_ON(unlikely(((instance_name == NULL) || (name == NULL)
		|| (value == NULL) || (value_len == NULL))))) {
		retval = -EINVAL;
		goto exit;
	}

	retval = handle->get(handle, instance_name,
						name, value, value_len, NULL);

exit:
	if (retval < 0) {
		printk(KERN_ERR "nameserver_remote_get failed! status = 0x%x",
			retval);
	}
	return retval;
}
