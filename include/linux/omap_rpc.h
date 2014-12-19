/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2011 Texas Instruments. All rights reserved.
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

#ifndef _OMAP_RPC_H_
#define _OMAP_RPC_H_

#include <linux/ioctl.h>

#if defined(CONFIG_ION_OMAP)
#include <linux/ion.h>
#endif

#define OMAPRPC_IOC_MAGIC			'O'

#define OMAPRPC_IOC_CREATE		_IOW(OMAPRPC_IOC_MAGIC, 1, char *)
#define OMAPRPC_IOC_DESTROY		_IO(OMAPRPC_IOC_MAGIC, 2)
#define OMAPRPC_IOC_IONREGISTER		_IOWR(OMAPRPC_IOC_MAGIC, 3, \
						struct ion_fd_data)
#define OMAPRPC_IOC_IONUNREGISTER	_IOWR(OMAPRPC_IOC_MAGIC, 4, \
						struct ion_fd_data)

#define OMAPRPC_IOC_MAXNR		   (4)

struct omaprpc_create_instance_t {
	char name[48];
};

struct omaprpc_channel_info_t {
	char name[64];
};

enum omaprpc_info_type_e {
	/* The number of functions in the service instance */
	OMAPRPC_INFO_NUMFUNCS,
	/* The symbol name of the function */
	OMAPRPC_INFO_FUNC_NAME,
	/* The number of times a function has been called */
	OMAPRPC_INFO_NUM_CALLS,
	/* The performance information releated to the function */
	OMAPRPC_INFO_FUNC_PERF,

	/* used to define the maximum info type */
	OMAPRPC_INFO_MAX
};

struct omaprpc_query_instance_t {
	uint32_t info_type;		/* omaprpc_info_type_e */
	uint32_t func_index;		/* The index to querty */
};

struct omaprpc_func_perf_t {
	uint32_t clocks_per_sec;
	uint32_t clock_cycles;
};

/** These core are specific to OMAP processors */
enum omaprpc_core_e {
	OMAPRPC_CORE_DSP = 0,	/* DSP Co-processor */
	OMAPRPC_CORE_SIMCOP,	/* Video/Imaging Co-processor */
	OMAPRPC_CORE_MCU0,	/* Cortex M3/M4 [0] */
	OMAPRPC_CORE_MCU1,	/* Cortex M3/M4 [1] */
	OMAPRPC_CORE_EVE,	/* Imaging Accelerator */
	OMAPRPC_CORE_REMOTE_MAX
};

struct omaprpc_instance_info_t {
	uint32_t info_type;
	uint32_t func_index;
	union info {
		uint32_t num_funcs;
		uint32_t num_calls;
		uint32_t core_index; /* omaprpc_core_e */
		char func_name[64];
		struct omaprpc_func_perf_t perf;
	} info;
};

enum omaprpc_cache_ops_e {
	OMAPRPC_CACHEOP_NONE = 0,
	OMAPRPC_CACHEOP_FLUSH,
	OMAPRPC_CACHEOP_INVALIDATE,

	OMAPRPC_CACHEOP_MAX,
};

struct omaprpc_param_translation_t {
	/* The parameter index which indicates which is the base pointer */
	uint32_t  index;
	/* The offset from the base address to the pointer to translate */
	ptrdiff_t offset;
	/*
	 * The base user virtual address of the pointer to
	 * translate (used to calc translated pointer offset).
	 */
	size_t base;
	/* The enumeration of desired cache operations for efficiency */
	uint32_t cacheOps;
	/* Reserved field */
	size_t reserved;
};

enum omaprpc_param_e {
	OMAPRPC_PARAM_TYPE_UNKNOWN = 0,
	/* An atomic data type, 1 byte to architecture limit sized bytes */
	OMAPRPC_PARAM_TYPE_ATOMIC,
	/*
	 * A pointer to shared memory. The reserved field
	 * must contain the handle to the memory
	 */
	OMAPRPC_PARAM_TYPE_PTR,
	/*
	 * (Unsupported) A structure type. Will be
	 * architecure width aligned in memory.
	 */
	OMAPRPC_PARAM_TYPE_STRUCT,
};

struct omaprpc_param_t {
	uint32_t type;		/* omaprpc_param_e */
	size_t size;		/* The size of the data */
	size_t data;		/* Either the pointer to the data or
				the data itself */
	size_t base;		/* If a pointer is in data, this is the base
				pointer (if data has an offset from base). */
	size_t reserved;	/* Shared Memory Handle
				(used only with pointers) */
};

#define OMAPRPC_MAX_PARAMETERS (10)

struct omaprpc_call_function_t {
	/* The function to call */
	uint32_t func_index;
	/* The number of parameters in the array. */
	uint32_t num_params;
	/* The array of parameters */
	struct omaprpc_param_t params[OMAPRPC_MAX_PARAMETERS];
	/* The number of translations needed in the offsets array */
	uint32_t num_translations;
	/*
	 * An indeterminate lenght array of offsets within
	 * payload_data to pointers which need translation
	 */
	struct omaprpc_param_translation_t translations[0];
};

#define OMAPRPC_MAX_TRANSLATIONS	(1024)

struct omaprpc_function_return_t {
	uint32_t func_index;
	uint32_t status;
};

#ifdef __KERNEL__

/* The applicable types of messages that the HOST may send the SERVICE. */
enum omaprpc_msg_type_e {
	/*
	 * Ask the ServiceMgr to create a new instance of the service.
	 * No secondary data is needed.
	 */
	OMAPRPC_MSG_CREATE_INSTANCE = 0,
	/*
	 * The return message from OMAPRPC_CREATE_INSTANCE,
	 * contains the new endpoint address in the omaprpc_instance_handle_t
	 */
	OMAPRPC_MSG_INSTANCE_CREATED = 1,
	/* Ask the Service Instance to send information about the Service */
	OMAPRPC_MSG_QUERY_INSTANCE = 2,
	/*
	 * The return message from OMAPRPC_QUERY_INSTANCE,
	 * which contains the information about the instance
	 */
	OMAPRPC_MSG_INSTANCE_INFO = 3,
	/* Ask the Service Mgr to destroy an instance */
	OMAPRPC_MSG_DESTROY_INSTANCE = 4,
	/* Ask the Service Instance to call a particular function */
	OMAPRPC_MSG_CALL_FUNCTION = 5,
	/*
	 * The return message from OMAPRPC_DESTROY_INSTANCE.
	 * contains the old endpoint address in the omaprpc_instance_handle_t
	 */
	OMAPRPC_MSG_INSTANCE_DESTROYED = 6,
	/*
	 * Returned from either the ServiceMgr or Service Instance
	 * when an error occurs
	 */
	OMAPRPC_MSG_ERROR = 7,
	/* The return values from a function call */
	OMAPRPC_MSG_FUNCTION_RETURN = 8,
	/* Ask Service for channel information*/
	OMAPRPC_MSG_QUERY_CHAN_INFO = 9,
	/* The return message from OMAPRPC_MSG_QUERY_CHAN_INFO*/
	OMAPRPC_MSG_CHAN_INFO = 10,

	/* used to define the max msg enum, not an actual message */
	OMAPRPC_MSG_MAX
};

enum omaprpc_state {
	/* No errors, just not initialized */
	OMAPRPC_STATE_DISCONNECTED,
	/* No errors, initialized remote DVP KGM */
	OMAPRPC_STATE_CONNECTED,
	/* Some error has been detected. Disconnected. */
	OMAPRPC_STATE_FAULT,

	/* Last item in enum */
	OMAPRPC_STATE_MAX
};

/*The generic OMAPRPC message header */
struct omaprpc_msg_header_t {
	uint32_t msg_type;	/* omaprpc_msg_type_e */
	uint32_t msg_flags;	/* Unused */
	uint32_t msg_len;	/* The length of the message data in bytes */
	uint8_t  msg_data[0];
} __packed;

struct omaprpc_instance_handle_t {
	uint32_t endpoint_address;
	uint32_t status;
} __packed;

struct omaprpc_error_t {
	uint32_t endpoint_address;
	uint32_t status;
} __packed;

struct omaprpc_parameter_t {
	 size_t size;
	 size_t data;
} __packed;

#define OMAPRPC_NUM_PARAMETERS(size) \
	(size/sizeof(struct omaprpc_parameter_t))

#define OMAPRPC_PAYLOAD(ptr, type) \
	((struct type *)&(ptr)[sizeof(struct omaprpc_msg_header_t)])

enum _omaprpc_translation_direction_e {
	OMAPRPC_UVA_TO_RPA,
	OMAPRPC_RPA_TO_UVA,
};

#endif /* __KERNEL__ */

#define OMAPRPC_DESC_EXEC_SYNC	(0x0100)
#define OMAPRPC_DESC_EXEC_ASYNC (0x0200)
#define OMAPRPC_DESC_SYM_ADD	(0x0300)
#define OMAPRPC_DESC_SYM_IDX	(0x0400)
#define OMAPRPC_DESC_CMD		(0x0500)
#define OMAPRPC_DESC_TYPE_MASK	(0x0F00)
#define OMAPRPC_JOBID_DISCRETE	(0)
#define OMAPRPC_POOLID_DEFAULT	(0x8000)

#define OMAPRPC_SET_FXN_IDX(idx) (idx | 0x80000000)
#define OMAPRPC_FXN_MASK(idx)	 (idx & 0x7FFFFFFF)

/** This is actually a frankensteined structure of RCM */
struct omaprpc_packet_t {
	uint16_t desc;		/* RcmClient_Packet.desc */
	uint16_t msg_id;	/* RcmClient_Packet.msgId */
	uint16_t pool_id;	/* RcmClient_Message.poolId */
	uint16_t job_id;	/* RcmClient_Message.jobId */
	uint32_t fxn_idx;	/* RcmClient_Message.fxnIdx */
	int32_t result;		/* RcmClient_Message.result */
	uint32_t data_size;	/* RcmClient_Message.data_size */
	uint8_t  data[0];	/* RcmClient_Message.data pointer */
} __packed;

#endif /* _OMAP_RPC_H_ */
