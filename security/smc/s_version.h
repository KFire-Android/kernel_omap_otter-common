/*
 * Copyright (c) 2007-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This software is the confidential and proprietary information of
 * Trusted Logic S.A. ("Confidential Information"). You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered
 * into with Trusted Logic S.A.
 *
 * TRUSTED LOGIC S.A. MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
 * SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT. TRUSTED LOGIC S.A. SHALL
 * NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING,
 * MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 */
#ifndef __S_VERSION_H__
#define __S_VERSION_H__

/*
 * Usage: define S_VERSION_BUILD on the compiler's command line.
 *
 * Then set:
 * - S_VERSION_OS
 * - S_VERSION_PLATFORM
 * - S_VERSION_MAJOR
 * - S_VERSION_MINOR
 * - S_VERSION_ENG is optional
 * - S_VERSION_PATCH is optional
 * - S_VERSION_BUILD = 0 if S_VERSION_BUILD not defined or empty
 */

#define S_VERSION_OS "A"          /* "A" for all Android */
#define S_VERSION_PLATFORM "G"    /* "G" for 4430 */

/*
 * This version number must be updated for each new release
 */
#define S_VERSION_MAJOR 1
#define S_VERSION_MINOR 1
/*
* If this is a patch or engineering version use the following
* defines to set the version number. Else set these values to 0.
*/
#define S_VERSION_ENG 0
#define S_VERSION_PATCH 0

#ifdef S_VERSION_BUILD
/* TRICK: detect if S_VERSION is defined but empty */
#if 0 == S_VERSION_BUILD-0
#undef  S_VERSION_BUILD
#define S_VERSION_BUILD 0
#endif
#else
/* S_VERSION_BUILD is not defined */
#define S_VERSION_BUILD 0
#endif

#if S_VERSION_ENG != 0
#define _S_VERSION_ENG "e" __STRINGIFY2(S_VERSION_ENG)
#else
#define _S_VERSION_ENG ""
#endif

#if S_VERSION_PATCH != 0
#define _S_VERSION_PATCH "p" __STRINGIFY2(S_VERSION_PATCH)
#else
#define _S_VERSION_PATCH ""
#endif

#define __STRINGIFY(X) #X
#define __STRINGIFY2(X) __STRINGIFY(X)

#if !defined(NDEBUG) || defined(_DEBUG)
#define S_VERSION_VARIANT "D   "
#else
#define S_VERSION_VARIANT "    "
#endif

#define S_VERSION_STRING \
	"SMC " \
	S_VERSION_OS \
	S_VERSION_PLATFORM \
	__STRINGIFY2(S_VERSION_MAJOR) "." \
	__STRINGIFY2(S_VERSION_MINOR) \
	_S_VERSION_ENG \
	_S_VERSION_PATCH \
	"."  __STRINGIFY2(S_VERSION_BUILD) " " \
	S_VERSION_VARIANT

#endif /* __S_VERSION_H__ */
