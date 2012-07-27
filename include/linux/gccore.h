/*
 * gccore.h
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef GCCORE_H
#define GCCORE_H

#include "sched.h"
#include "gcioctl.h"

/* Capability query. */
void gc_caps(struct gcicaps *gcicaps);

/* Command buffer submission. */
void gc_commit(struct gcicommit *gcicommit, bool fromuser);

/* Client memory mapping. */
void gc_map(struct gcimap *gcimap, bool fromuser);
void gc_unmap(struct gcimap *gcimap, bool fromuser);

/* Arm a callback. */
void gc_callback(struct gcicallbackarm *gcicallbackarm, bool fromuser);

/* Process cleanup. */
void gc_release(void);

#endif
