/*
 *  nameserver_remotenotify.h
 *
 *  The nameserver_remotenotify module provides functionality to get name
 *  value pair from a remote nameserver.
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

#ifndef _NAMESERVER_REMOTENOTIFY_H_
#define _NAMESERVER_REMOTENOTIFY_H_

#include <linux/types.h>

/*
 *  NAMESERVERREMOTENOTIFY_MODULEID
 *  Unique module ID
 */
#define NAMESERVERREMOTENOTIFY_MODULEID      (0x08FD)

/*
 *  Module configuration structure
 */
struct nameserver_remotenotify_config {
	u32 notify_event_id;
	/* Notify event number */
};

/*
  * Module configuration structure
 */
struct nameserver_remotenotify_params {
	void *shared_addr; /* Address of the shared memory */
	void *gatemp; /* Handle to the gatemp used for protecting the
			nameserver_remotenotify instance. Using the default
			value of NULL will result in the default gatemp being
			used for context protection */
};

/* Function to get the default configuration for the nameserver_remotenotify
 * module */
void nameserver_remotenotify_get_config(
				struct nameserver_remotenotify_config *cfg);

/* Function to setup the nameserver_remotenotify module */
int nameserver_remotenotify_setup(struct nameserver_remotenotify_config *cfg);

/* Function to destroy the nameserver_remotenotify module */
int nameserver_remotenotify_destroy(void);

/* Function to get the current configuration values */
void nameserver_remotenotify_params_init(
				struct nameserver_remotenotify_params *params);

/* Function to create the nameserver_remotenotify object */
void *nameserver_remotenotify_create(u16 remote_proc_id,
			const struct nameserver_remotenotify_params *params);

/* Function to delete the nameserver_remotenotify object */
int nameserver_remotenotify_delete(void **handle);

/* Function to get a name/value from remote nameserver */
int nameserver_remotenotify_get(void *handle,
				const char *instance_name, const char *name,
				void *value, u32 *value_len, void *reserved);

/* Get the shared memory requirements for the nameserver_remotenotify */
uint nameserver_remotenotify_shared_mem_req(
			const struct nameserver_remotenotify_params *params);

/* Create all the NameServerRemoteNotify drivers. */
int nameserver_remotenotify_start(void *shared_addr);

/* Attaches to remote processor */
int nameserver_remotenotify_attach(u16 remote_proc_id, void *shared_addr);

/* Detaches from remote processor */
int nameserver_remotenotify_detach(u16 remote_proc_id);


#endif /* _NAMESERVER_REMOTENOTIFY_H_ */
