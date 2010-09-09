/*
 *  platform.h
 *
 *  Defines the platform functions to be used by SysMgr module.
 *
 *  Copyright (C) 2009 Texas Instruments, Inc.
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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#define PLATFORM_S_SUCCESS       0
#define PLATFORM_E_FAIL         -1
#define PLATFORM_E_INVALIDARG   -2

/* =============================================================================
 * APIs
 * =============================================================================
 */
/* Function to setup the platform */
s32 platform_setup(void);

/* Function to destroy the platform */
s32 platform_destroy(void);

/* Function called when slave is loaded with executable */
int platform_load_callback(u16 proc_id, void *arg);

/* Function called when slave is in started state*/
int platform_start_callback(u16 proc_id, void *arg);

/* Function called when slave is stopped state */
int platform_stop_callback(u16 proc_id, void *arg);

#endif /* ifndef _PLATFORM_H_ */
