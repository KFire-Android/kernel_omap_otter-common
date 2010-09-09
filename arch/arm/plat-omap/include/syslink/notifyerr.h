/*
 * notifyerr.h
 *
 * Notify driver support for OMAP Processors.
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#if !defined NOTIFYERR_H
#define NOTIFYERR_H


/*
 *  name   NOTIFY_SUCCEEDED
 *
 *  desc   Check if the provided status code indicates a success code.
 *
 *  arg    status
 *              Status code to be checked
 *
 *  ret    TRUE
 *              If status code indicates success
 *          FALSE
 *              If status code indicates failure
 *
 *  enter  None.
 *
 *  leave  None.
 *
 *  see    NOTIFY_FAILED
 *
 */
#define NOTIFY_SUCCEEDED(status)\
(((signed long int) (status) >= (NOTIFY_SBASE)) \
&& ((signed long int) (status) <= (NOTIFY_SLAST)))


/*
 *  @name   NOTIFY_FAILED
 *
 *  @desc   Check if the provided status code indicates a failure code.
 *
 *  @arg    status
 *              Status code to be checked
 *
 *  @ret    TRUE
 *              If status code indicates failure
 *          FALSE
 *              If status code indicates success
 *
 *  @enter  None.
 *
 *  @leave  None.
 *
 *  @see    NOTIFY_FAILED
 *
 */
#define NOTIFY_FAILED(status)      (!NOTIFY_SUCCEEDED(status))



/*
 *  name   NOTIFY_SBASE, NOTIFY_SLAST
 *
 *   desc   Defines the base and range for the success codes used by the
 *          Notify module
 *
 */
#define NOTIFY_SBASE               (signed long int)0x00002000l
#define NOTIFY_SLAST               (signed long int)0x000020FFl

/*
 *  name   NOTIFY_EBASE, NOTIFY_ELAST
 *
 *  desc   Defines the base and range for the failure codes used by the
 *          Notify module
 *
 */
#define NOTIFY_EBASE               (signed long int)0x80002000l
#define NOTIFY_ELAST               (signed long int)0x800020FFl


/*
 *  SUCCESS Codes
 *
 */

/* Generic success code for Notify module */
#define NOTIFY_SOK                (NOTIFY_SBASE + 0x01l)

/* Indicates that the Notify module (or driver) has already been initialized
 * by another client, and this process has now successfully acquired the right
 * to use the Notify module.
 */
#define NOTIFY_SALREADYINIT       (NOTIFY_SBASE + 0x02l)

/* Indicates that the Notify module (or driver) is now being finalized, since
  * the calling client is the last one finalizing the module, and all open
 * handles to it have been closed.
 */
#define NOTIFY_SEXIT              (NOTIFY_SBASE + 0x03l)


/*
 *  FAILURE Codes
 *
 */

/* Generic failure code for Notify module */
#define NOTIFY_EFAIL              (NOTIFY_EBASE + 0x01l)

/* This failure code indicates that an operation has timed out. */
#define NOTIFY_ETIMEOUT           (NOTIFY_EBASE + 0x02l)

/* This failure code indicates a configuration error */
#define NOTIFY_ECONFIG            (NOTIFY_EBASE + 0x03l)

/* This failure code indicates that the Notify module has already been
 * initialized from the calling client (process).
 */
#define NOTIFY_EALREADYINIT       (NOTIFY_EBASE + 0x04l)

/* This failure code indicates that the specified entity was not found
 * The interpretation of this error code depends on the function from which it
 * was returned.
 */
#define NOTIFY_ENOTFOUND          (NOTIFY_EBASE + 0x05l)

/* This failure code indicates that the specified feature is not supported
 * The interpretation of this error code depends on the function from which it
 * was returned.
 */
#define NOTIFY_ENOTSUPPORTED      (NOTIFY_EBASE + 0x06l)

/* This failure code indicates that the specified event number is
 * not supported
 */
#define NOTIFY_EINVALIDEVENT      (NOTIFY_EBASE + 0x07l)

/* This failure code indicates that the specified pointer is invalid */
#define NOTIFY_EPOINTER           (NOTIFY_EBASE + 0x08l)

/* This failure code indicates that a provided parameter was outside its valid
 * range.
 * The interpretation of this error code depends on the function from which it
 * was returned.
 */
#define NOTIFY_ERANGE             (NOTIFY_EBASE + 0x09l)

/* This failure code indicates that the specified handle is invalid */
#define NOTIFY_EHANDLE            (NOTIFY_EBASE + 0x0Al)

/* This failure code indicates that an invalid argument was specified */
#define NOTIFY_EINVALIDARG        (NOTIFY_EBASE + 0x0Bl)

/* This failure code indicates a memory related failure */
#define NOTIFY_EMEMORY            (NOTIFY_EBASE + 0x0Cl)

/* This failure code indicates that the Notify module has not been initialized*/
#define NOTIFY_EINIT              (NOTIFY_EBASE + 0x0Dl)

/* This failure code indicates that a resource was not available.
 * The interpretation of this error code depends on the function from which it
 * was returned.
 */
#define NOTIFY_ERESOURCE          (NOTIFY_EBASE + 0x0El)

/* This failure code indicates that there was an attempt to register for a
 * reserved event.
 */
#define NOTIFY_ERESERVEDEVENT     (NOTIFY_EBASE + 0x0Fl)

/* This failure code indicates that the specified entity already exists.
 * The interpretation of this error code depends on the function from which it
 * was returned.
 */
#define NOTIFY_EALREADYEXISTS     (NOTIFY_EBASE + 0x10l)

/* This failure code indicates that the Notify driver has not been fully
 * initialized
 */
#define NOTIFY_EDRIVERINIT        (NOTIFY_EBASE + 0x11l)

/* This failure code indicates that the other side is not ready to receive
 * notifications.
 */
#define NOTIFY_ENOTREADY          (NOTIFY_EBASE + 0x12l)

#endif /* !defined (NOTIFYERR_H) */
