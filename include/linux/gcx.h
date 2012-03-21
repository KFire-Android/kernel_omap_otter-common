/*
 * gcx.h
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

#ifndef GCX_H
#define GCX_H

#include "gcerror.h"
#include "gcreg.h"

/* Debug print prefixes. */
#define GC_MOD_PREFIX	GC_DEV_NAME ": %s(%d) "

/* Debug macro stubs. */
#ifndef GCDEBUG_ENABLE
#	define GCDEBUG_ENABLE 0
#endif

#ifndef GCGPUSTATUS
#	define GCGPUSTATUS(filter, zone, function, line, acknowledge)
#endif

#ifndef GCPRINT
#	define GCPRINT(...)
#endif

#ifndef GCDUMPBUFFER
#	define GCDUMPBUFFER(filter, zone, ptr, gpuaddr, datasize)
#endif

#endif
