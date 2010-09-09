/*
 *  gate.h
 *
 *  Critical section support.
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
/** ============================================================================
 *  Gates are used by clients to protect concurrent access to critical
 *  data structures.  Critical data structures are those that must be
 *  updated by at most one thread at a time.  All code that needs access
 *  to a critical data structure "enters" a gate (that's associated with the
 *  data structure) prior to accessing the data, modifies the data structure,
 *  then "leaves" the gate.
 *
 *  A gate is responsible for ensuring that at most one thread at a time
 *  can enter and execute "inside" the gate.  There are several
 *  implementations of gates, with different system executation times and
 *  latency tradoffs.  In addition, some gates must not be entered by certain
 *  thread types; e.g., a gate that is implemented via a "blocking" semaphore
 *  must not be called by an interrupt service routine (ISR).
 *
 *  A module can be declared "gated" by adding the `@Gated` attribute to the
 *  module's XDC spec file.  A "gated" module is assigned a module-level gate
 *  at the configuration time, and that gate is then used to protect critical
 *  sections in the module's target code. A module-level gate is an instance of
 *  a module implementing `{@link IGateProvider}` interface. However, gated
 *  modules do not access their module-level gates directly. They use this
 *  module to access transparently their module-level gate.
 *
 *  Application code that is not a part of any module also has a
 *  module-level gate, configured through the module `{@link Main}`.
 *
 *  Each gated module can optionally create gates on an adhoc basis at
 *  runtime using the same gate module that was used to create the module
 *  level gate.
 *
 *  Gates that work by disabling all preemption while inside a gate can be
 *  used to protect data structures accessed by ISRs and other
 *  threads.  But, if the time required to update the data structure is not
 *  a small constant, this type of gate may violate a system's real-time
 *  requirements.
 *
 *  Gates have two orthogonal attributes: "blocking" and "preemptible".
 *  In general, gates that are "blocking" can not be use by code that is
 *  called by ISRs and gates that are not "preemptible" should only be used to
 *  to protect data manipulated by code that has small constant execution
 *  time.
 *  ============================================================================
 */


#ifndef GATE_H_0xAF6F
#define GATE_H_0xAF6F

#include <igateprovider.h>

extern struct igateprovider_object *gate_system_handle;

/* Function to enter a Gate */
int *gate_enter_system(void);

/* Function to leave a Gate */
void gate_leave_system(int *key);


#endif /* GATE_H_0xAF6F */
