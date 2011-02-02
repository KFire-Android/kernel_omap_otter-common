/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 *		Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#ifndef _ABE_MAIN_H_
#define _ABE_MAIN_H_

#include <linux/io.h>

#include "abe_dm_addr.h"
#include "abe_sm_addr.h"
#include "abe_cm_addr.h"
#include "abe_define.h"
#include "abe_fw.h"
#include "abe_def.h"
#include "abe_typ.h"
#include "abe_ext.h"
#include "abe_dbg.h"
#include "abe_lib.h"
#include "abe_api.h"
#include "abe_typedef.h"
#include "abe_functionsid.h"
#include "abe_taskid.h"
#include "abe_initxxx_labels.h"
#include "abe_fw.h"

/* pipe connection to the TARGET simulator */
#define ABE_DEBUG_CHECKERS              0
/* simulator data extracted from a text-file */
#define ABE_DEBUG_HWFILE                0
/* low-level log files */
#define ABE_DEBUG_LL_LOG                0

#endif				/* _ABE_MAIN_H_ */
