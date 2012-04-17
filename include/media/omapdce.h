/*
 * include/drm/omap_drm.h
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Rob Clark <rob.clark@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OMAP_DCE_H__
#define __OMAP_DCE_H__

/* Please note that modifications to all structs defined here are subject to
 * backwards-compatibility constraints.
 *
 * Cacheability and control structs:
 * ------------ --- ------- --------
 * All the ioctl params w/ names ending in _bo are GEM buffer object handles
 * for control structures that are shared with the coprocessor.  You probably
 * want to create them as uncached or writecombine.
 *
 * NOTE: The DCE ioctl calls are synchronous, and the coprocessor will not
 * access the control structures passed as parameters other than during
 * the duration of the ioctl call that they are passed to.  So if we avoid
 * adding asynchronous ioctl calls (or if we do, if userspace brackets CPU
 * access to the control structures with OMAP_GEM_CPU_PREP/OMAP_GEM_CPU_FINI),
 * we could handle the necessary clean/invalidate ops in the ioctl handlers.
 * But for now be safe and use OMAP_BO_WC.
 *
 * About resuming interrupted ioctl calls:
 * ----- -------- ----------- ----- ------
 * In the ioctl command structs which return values, there is a 'token'
 * field.  Userspace should initially set this value to zero.  If the
 * syscall is interrupted, the driver will set a (file-private) token
 * value, and userspace should loop and re-start the ioctl returning the
 * token value that the driver set.  This allows the driver to realize
 * that it is a restarted syscall, and that it should simply wait for a
 * response from coprocessor rather than starting a new request.
 */

enum omap_dce_codec {
	OMAP_DCE_VIDENC2 = 1,
	OMAP_DCE_VIDDEC3 = 2,
};

struct omap_dce_engine_open {
	char name[32];			/* engine name (in) */
	int32_t error_code;		/* error code (out) */
	uint32_t eng_handle;	/* engine handle (out) */
	uint32_t token;
};

struct omap_dce_engine_close {
	uint32_t eng_handle;	/* engine handle (in) */
	uint32_t __pad;
};

struct omap_dce_codec_create {
	uint32_t codec_id;		/* enum omap_dce_codec (in) */
	uint32_t eng_handle;	/* engine handle (in) */
	char name[32];			/* codec name (in) */
	uint32_t sparams_bo;	/* static params (in) */
	uint32_t codec_handle;	/* codec handle, zero if failed (out) */
	uint32_t token;
};

struct omap_dce_codec_control {
	uint32_t codec_handle;	/* codec handle (in) */
	uint32_t cmd_id;		/* control cmd id (in) */
	uint32_t dparams_bo;	/* dynamic params (in) */
	uint32_t status_bo;		/* status (in) */
	int32_t result;			/* return value (out) */
	uint32_t token;
};

struct omap_dce_codec_process {
	uint32_t codec_handle;	/* codec handle (in) */
	uint32_t out_args_bo;	/* output args (in) */
	uint64_t in_args;		/* input args ptr (in) */
	uint64_t out_bufs;		/* output buffer-descriptor ptr (in) */
	uint64_t in_bufs;		/* input buffer-descriptor ptr (in) */
	int32_t result;			/* return value (out) */
	uint32_t token;
};

struct omap_dce_codec_delete {
	uint32_t codec_handle;	/* codec handle (in) */
	uint32_t __pad;
};

#define XDM_MEMTYPE_BO 10

#define OMAP_DCE_ENGINE_OPEN	_IOWR('z', 100, struct omap_dce_engine_open)
#define OMAP_DCE_ENGINE_CLOSE	_IOW ('z', 101, struct omap_dce_engine_close)
#define OMAP_DCE_CODEC_CREATE	_IOWR('z', 102, struct omap_dce_codec_create)
#define OMAP_DCE_CODEC_CONTROL	_IOWR('z', 103, struct omap_dce_codec_control)
#define OMAP_DCE_CODEC_PROCESS	_IOWR('z', 104, struct omap_dce_codec_process)
#define OMAP_DCE_CODEC_DELETE	_IOW ('z', 105, struct omap_dce_codec_delete)

#endif /* __OMAP_DCE_H__ */
