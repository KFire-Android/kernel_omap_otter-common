/*
 * SdioDrvDbg.h
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Texas Instruments nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OMAP3430_SDIODRV_DEBUG_H
#define OMAP3430_SDIODRV_DEBUG_H

#include <linux/kernel.h>

typedef enum{
SDIO_DEBUGLEVEL_EMERG=1,
SDIO_DEBUGLEVEL_ALERT,
SDIO_DEBUGLEVEL_CRIT,
SDIO_DEBUGLEVEL_ERR=4,
SDIO_DEBUGLEVEL_WARNING,
SDIO_DEBUGLEVEL_NOTICE,
SDIO_DEBUGLEVEL_INFO,
SDIO_DEBUGLEVEL_DEBUG=8
}sdio_debuglevel;

extern int g_sdio_debug_level;

#ifdef SDIO_DEBUG

#define PERR(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_ERR) printk(format , ##args)
#define PDEBUG(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_DEBUG) printk(format , ##args)
#define PINFO(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_INFO) printk( format , ##__VA_ARGS__)
#define PNOTICE(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_NOTICE) printk( format , ##__VA_ARGS__)
#define PWARNING(format, ... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_WARNING) printk(format , ##__VA_ARGS__)

#else

#define PERR(format, args... ) if(g_sdio_debug_level >= SDIO_DEBUGLEVEL_ERR) printk(format , ##args)
#define PDEBUG(format, args... )
#define PINFO(format, ... )
#define PNOTICE(format, ... )
#define PWARNING(format, ... )

#endif

/* we want errors reported anyway */

#define PERR1 PERR
#define PERR2 PERR
#define PERR3 PERR

#endif /* OMAP3430_SDIODRV_DEBUG_H */
