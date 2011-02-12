/*
 * chnlobj.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Structure subcomponents of channel class library channel objects which
 * are exposed to class driver from mini-driver.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef CHNLOBJ_
#define CHNLOBJ_

#include <dspbridge/chnldefs.h>
#include <dspbridge/wmd.h>

/*
 *  This struct is the first field in a chnl_mgr struct, as implemented in
 *  a WMD channel class library.  Other, implementation specific fields
 *  follow this structure in memory.
 */
struct chnl_mgr_ {
	/* These must be the first fields in a chnl_mgr struct: */
	struct bridge_drv_interface *intf_fxns;	/* Function interface to WMD. */
};

/*
 *  This struct is the first field in a chnl_object struct, as implemented in
 *  a WMD channel class library.  Other, implementation specific fields
 *  follow this structure in memory.
 */
struct chnl_object_ {
	/* These must be the first fields in a chnl_object struct: */
	struct chnl_mgr_ *chnl_mgr_obj;	/* Pointer back to channel manager. */
};

#endif /* CHNLOBJ_ */
