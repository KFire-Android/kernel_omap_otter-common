/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __S_VERSION_H__
#define __S_VERSION_H__

/*
 * Usage: define S_VERSION_BUILD on the compiler's command line.
 *
 * Then, you get:
 * - S_VERSION_MAJOR
 * - S_VERSION_MINOR
 * - S_VERSION_MICRO
 * - S_VERSION_BUILD = 0 if S_VERSION_BUILD not defined or empty
 * - S_VERSION_STRING = "SMC X.Y.Z.N     " or "SMC X.Y.Z.N D   "
 * - S_VERSION_RESOURCE = X,Y,Z,N
 */

/*
 * This master version number must be updated for each new release
 */
#define S_VERSION_MAJOR 2
#define S_VERSION_MINOR 5
#define S_VERSION_MICRO 3

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

#define __STRINGIFY(X) #X
#define __STRINGIFY2(X) __STRINGIFY(X)

#if !defined(NDEBUG) || defined(_DEBUG)
#define S_VERSION_VARIANT "D   "
#else
#define S_VERSION_VARIANT "    "
#endif

#define S_VERSION_STRING \
	 "SMC " \
	 __STRINGIFY2(S_VERSION_MAJOR) "." \
	 __STRINGIFY2(S_VERSION_MINOR) "." \
	 __STRINGIFY2(S_VERSION_MICRO) "." \
	 __STRINGIFY2(S_VERSION_BUILD) " " \
	 S_VERSION_VARIANT

#define S_VERSION_RESOURCE (S_VERSION_MAJOR, S_VERSION_MINOR, \
				S_VERSION_MICRO, S_VERSION_BUILD)

#endif /* __S_VERSION_H__ */
