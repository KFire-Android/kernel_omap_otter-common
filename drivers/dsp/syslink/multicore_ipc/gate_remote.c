/*
 *  gate_remote.c
 *
 *  This includes the functions to handle remote gates
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

/*
 * ======== gate_remote_enter ========
 *  Purpose:
 *  This function is used to enter in to a remote gate
 */
int gate_remote_enter(void *ghandle)
{
	return 0;
}

/*
 * ======== gate_remote_leave ========
 *  Purpose:
 *  This function is used to leave from a remote gate
 */
int gate_remote_leave(void *ghandle, u32 key)
{
	key = 0;
	return 0;
}
