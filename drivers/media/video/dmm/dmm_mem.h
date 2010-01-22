/*
 * dmm_mem.h
 *
 * DMM driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef DMM_MEM_H
#define DMM_MEM_H

/**
 * Initialize the DMM page stacks.
 * @return an error status.
 */
u32 dmm_init_mem(void);

/**
 * Free the DMM page stacks.
 */
void dmm_release_mem(void);

#endif
