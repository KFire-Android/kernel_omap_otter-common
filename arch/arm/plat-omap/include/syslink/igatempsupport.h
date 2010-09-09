/*
 *  igatempsupport.h
 *
 *  Interface implemented by all multiprocessor gates.
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
 *
 */

#ifndef _IGATEMPSUPPORT_H_
#define _IGATEMPSUPPORT_H_


/* Invalid Igate */
#define IGATEMPSUPPORT_NULL		(void *)0xFFFFFFFF

/* Gates with this "quality" may cause the calling thread to block;
 *  i.e., suspend execution until another thread leaves the gate. */
#define IGATEMPSUPPORT_Q_BLOCKING	1

/* Gates with this "quality" allow other threads to preempt the thread
 *  that has already entered the gate. */
#define IGATEMPSUPPORT_Q_PREEMPTING	2

/* Object embedded in other Gate modules. (Inheritance) */
#define IGATEMPSUPPORT_SUPERPARAMS		\
	u32 resource_id;			\
	bool open_flag;				\
	u16 region_id;				\
	void *shared_addr

/* All other GateMP modules inherit this. */
#define IGATEMPSUPPORT_INHERIT(X)		\
enum X##_local_protect {			\
	X##_LOCALPROTECT_NONE = 0,		\
	X##_LOCALPROTECT_INTERRUPT = 1,		\
	X##_LOCALPROTECT_TASKLET = 2,		\
	X##_LOCALPROTECT_THREAD = 3,		\
	X##_LOCALPROTECT_PROCESS = 4		\
};

/* Paramter initializer. */
#define IGATEMPSUPPORT_PARAMSINTIALIZER(x)	\
	do {					\
		(x)->resource_id = 0;		\
		(x)->open_flag = true;		\
		(x)->region_id = 0;		\
		(x)->shared_addr = NULL;	\
	} while (0)

enum igatempsupport_local_protect {
	IGATEMPSUPPORT_LOCALPROTECT_NONE = 0,
	IGATEMPSUPPORT_LOCALPROTECT_INTERRUPT = 1,
	IGATEMPSUPPORT_LOCALPROTECT_TASKLET = 2,
	IGATEMPSUPPORT_LOCALPROTECT_THREAD = 3,
	IGATEMPSUPPORT_LOCALPROTECT_PROCESS = 4
};

struct igatempsupport_params {
	u32 resource_id;
	bool open_flag;
	u16 region_id;
	void *shared_addr;
};


#endif /* ifndef __IGATEMPSUPPORT_H__ */
