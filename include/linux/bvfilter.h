/*
 * bvfilter.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * This file defines the types of shared filters available and the associated
 * parameters.
 *
 * To extend the list of filters, create a file containing additional
 * enumerations to be added to enum bvfilter below.  Then #define
 * BVFILTER_EXTERNAL_INCLUDE as the name of that file before including
 * this file in your project.  Parameters need to be in a different file.
 */

#ifndef BVFILTER_H
#define BVFILTER_H

/*
 * bvfilter is an enumeration used to designate the type of filter being used.
 */
enum bvfiltertype {
	BVFILTER_DUMMY
	/* TBD */

#ifdef BVFILTER_EXTERNAL_INCLUDE
#include BVFILTER_EXTERNAL_INCLUDE
#endif
};

/*
 * bvfilterop contains the filter type and a pointer to the associated
 * parameters when the BVFLAG_FILTER operation is specified in
 * bvbltparams.flags.
 */
struct bvfilter {
	enum bvfiltertype filter;
	void *params;
};

#endif /* BVFILTER_H */
