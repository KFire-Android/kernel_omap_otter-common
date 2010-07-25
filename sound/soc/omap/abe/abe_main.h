/*
 * ==========================================================================
 *               Texas Instruments OMAP(TM) Platform Firmware
 * (c) Copyright 2009, Texas Instruments Incorporated.  All Rights Reserved.
 *
 *  Use of this firmware is controlled by the terms and conditions found
 *  in the license agreement under which this firmware has been supplied.
 * ==========================================================================
 */

#ifndef _ABE_MAIN_H_
#define _ABE_MAIN_H_

#include "abe_dm_addr.h"
#include "abe_cm_addr.h"
#include "abe_def.h"
#include "abe_typ.h"
#include "abe_dbg.h"
#include "abe_ext.h"
#include "abe_lib.h"
#include "abe_ref.h"
#include "abe_api.h"
//#include "ABE_DAT.h"

#include "abe_typedef.h"
#include "abe_functionsId.h"
#include "abe_taskId.h"
#include "abe_dm_addr.h"
#include "abe_sm_addr.h"
#include "abe_cm_addr.h"
#include "abe_initxxx_labels.h"

#include "abe_fw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ABE_HAL_VERSION		0x00000002L
#define ABE_FW_VERSION		0x00000000L
#define ABE_HW_VERSION		0x00000000L

#define ABE_DEBUG_CHECKERS	0	/* pipe connection to the TARGET simulator */
#define ABE_DEBUG_HWFILE	0	/* simulator data extracted from a text-file */
#define ABE_DEBUG_LL_LOG	0	/* low-level log files */

#define ABE_DEBUG (ABE_DEBUG_CHECKERS | ABE_DEBUG_HWFILE | ABE_DEBUG_LL_LOG)

#ifdef __cplusplus
}
#endif

#endif	/* _ABE_MAIN_H_ */
