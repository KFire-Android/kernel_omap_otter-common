/*
 * drivers/staging/omapdce/dce_rpc.h
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

#ifndef __DCE_RPC_H__
#define __DCE_RPC_H__

/* RPC layer types.. these define the payload of messages between firmware
 * and linux side.  This should be kept in sync between firmware build and
 * driver.
 *
 * TODO: xxx_control(XDM_GETVERSION) is a bit awkward to deal with, because
 * this seems to be the one special case where status->data is used..
 * possibly we should define a special ioctl and msg to handle this case.
 */

/* Message-Ids:
 */
#define DCE_RPC_ENGINE_OPEN		0x00
#define DCE_RPC_ENGINE_CLOSE	0x01
#define DCE_RPC_CODEC_CREATE	0x02
#define DCE_RPC_CODEC_CONTROL	0x03
#define DCE_RPC_CODEC_PROCESS	0x04
#define DCE_RPC_CODEC_DELETE	0x05

struct dce_rpc_hdr {
	/* A Message-Id as defined above:
	 */
	uint16_t msg_id;

	/* request-id is assigned on host side, and echoed back from
	 * coprocessor to allow the host side to match up responses
	 * to requests:
	 */
	uint16_t req_id;
} __packed;

struct dce_rpc_engine_open_req {
	struct dce_rpc_hdr hdr;
	char name[32];
} __packed;

struct dce_rpc_engine_open_rsp {
	struct dce_rpc_hdr hdr;
	int32_t error_code;
	uint32_t engine;
} __packed;

struct dce_rpc_engine_close_req {
	struct dce_rpc_hdr hdr;
	uint32_t engine;
} __packed;

struct dce_rpc_codec_create_req {
	struct dce_rpc_hdr hdr;
	uint32_t codec_id;
	uint32_t engine;
	char name[32];
	uint32_t sparams;
} __packed;

struct dce_rpc_codec_create_rsp {
	struct dce_rpc_hdr hdr;
	uint32_t codec;
} __packed;

struct dce_rpc_codec_control_req {
	struct dce_rpc_hdr hdr;
	uint32_t codec_id;
	uint32_t codec;
	uint32_t cmd_id;
	uint32_t dparams;
	uint32_t status;
} __packed;

struct dce_rpc_codec_control_rsp {
	struct dce_rpc_hdr hdr;
	int32_t result;
} __packed;

/* NOTE: CODEC_PROCESS does somewhat more than the other ioctls, in that it
 * handles buffer mapping/unmapping.  So the inBufs/outBufs are copied inline
 * (with translated addresses in the copy sent inline with codec_process_req).
 * Since we need the inputID from inArgs, and it is a small struct, it is also
 * copied inline.
 *
 * Therefore, the variable length data[] section has the format:
 *    uint8_t inargs[in_args_length * 4];
 *    uint8_t outbufs[in_bufs_length * 4];
 *    uint8_t inbufs[in_bufs_length * 4];
 */
struct dce_rpc_codec_process_req {
	struct dce_rpc_hdr hdr;
	uint32_t codec_id;
	uint32_t codec;
	uint8_t pad;
	uint8_t in_args_len;    /* length/4 */
	uint8_t out_bufs_len;   /* length/4 */
	uint8_t in_bufs_len;    /* length/4 */
	uint32_t out_args;
	uint8_t data[];
} __packed;

struct dce_rpc_codec_process_rsp {
	struct dce_rpc_hdr hdr;
	int32_t result;
	uint8_t count;
	int32_t freebuf_ids[];
} __packed;

struct dce_rpc_codec_delete_req {
	struct dce_rpc_hdr hdr;
	uint32_t codec_id;
	uint32_t codec;
} __packed;

#endif /* __DCE_RPC_H__ */
