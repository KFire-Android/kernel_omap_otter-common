/*
 * drivers/staging/omapdce/dce.c
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

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/rpmsg.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/omap_ion.h>

#include <media/omapdce.h>

#include "dce_rpc.h"

enum {
	XDM_MEMTYPE_PVRFD = -1,
	XDM_MEMTYPE_ROW = 0,
	XDM_MEMTYPE_RAW = 0,
	XDM_MEMTYPE_TILED8 = 1,
	XDM_MEMTYPE_TILED16 = 2,
	XDM_MEMTYPE_TILED32 = 3,
	XDM_MEMTYPE_TILEDPAGE = 4
};


int debug_dce = 0;
module_param(debug_dce, int, 0644);

#define DBG(fmt,...)  if (debug_dce) printk(fmt"\n", ##__VA_ARGS__)
#define VERB(fmt,...) if (0) printk(fmt, ##__VA_ARGS__) /* verbose debug */

/* Removeme.. */
static long mark(long *last)
{
    struct timeval t;
    do_gettimeofday(&t);
    if (last) {
        return t.tv_usec - *last;
    }
    return t.tv_usec;
}

#define MAX_ENGINES	32
#define MAX_CODECS	32
#define MAX_LOCKED_BUFFERS	32  /* actually, 16 should be enough if reorder queue is disabled */
#define MAX_BUFFER_OBJECTS	((2 * MAX_LOCKED_BUFFERS) + 4)
#define MAX_TRANSACTIONS	MAX_CODECS

struct dce_device {
	struct cdev cdev;
};

struct dce_buffer {
	int32_t id;		/* zero for unused */
	struct drm_gem_object *y, *uv;
};

struct dce_engine {
	uint32_t engine;
};

struct dce_codec {
	enum omap_dce_codec codec_id;
	uint32_t codec;
	struct dce_buffer locked_buffers[MAX_LOCKED_BUFFERS];
};

struct dce_file_priv {
	struct file *file;

	/* NOTE: engine/codec end up being pointers (or similar) on
	 * coprocessor side.. so these are not exposed directly to
	 * userspace.  Instead userspace sees per-file unique handles
	 * which index into the table of engines/codecs.
	 */
	struct dce_engine engines[MAX_ENGINES];

	struct dce_codec codecs[MAX_CODECS];

	/* the token returned to userspace is an index into this table
	 * of msg request-ids.  This avoids any chance that userspace might
	 * try to guess another processes txn req_id and try to intercept
	 * the other processes reply..
	 */
	uint16_t req_ids[MAX_CODECS];
	atomic_t next_token;
};

/* per-transaction data.. indexed by req_id%MAX_REQUESTS
 */

struct omap_dce_txn {
	struct dce_file_priv *priv;  /* file currently using thix txn slot */
	struct dce_rpc_hdr *rsp;
	int len;
	int bo_count;
};
static struct omap_dce_txn txns[MAX_TRANSACTIONS];

/* note: eventually we could perhaps have per drm_device private data
 * and pull all this into a struct.. but in rpmsg_cb we don't have a
 * way to get at that, so for now all global..
 */
static struct rpmsg_channel *rpdev;
static atomic_t next_req_id = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(lock);  // TODO probably more locking needed..

static struct dce_device *dce_device;
static struct device *device;
static struct class *dcedev_class;
static s32 dce_major;
static s32 dce_minor;

/*
 * Utils:
 */

#define hdr(r) ((void *)(r))

/* initialize header and assign request id: */
#define MKHDR(x) (struct dce_rpc_hdr){ .msg_id = DCE_RPC_##x, .req_id = atomic_inc_return(&next_req_id) }

static uint32_t rpsend(struct dce_file_priv *priv, uint32_t *token,
		struct dce_rpc_hdr *req, int len)
{
	struct omap_dce_txn *txn =
			&txns[req->req_id % ARRAY_SIZE(txns)];

	WARN_ON(txn->priv);

	/* assign token: */
	if (token) {
		*token = atomic_inc_return(&priv->next_token) + 1;
		priv->req_ids[(*token-1) % ARRAY_SIZE(priv->req_ids)] = req->req_id;

		txn->priv = priv;

		// XXX  wait for paddrs to become valid!

	} else {
		/* message with no response: */
		req->req_id = 0xffff;	  /* just for debug */
		WARN_ON(txn->bo_count > 0);    /* this is not valid */

		memset(txn, 0, sizeof(*txn));
	}

	return rpmsg_send(rpdev, req, len);
}

static int rpwait(struct dce_file_priv *priv, uint32_t token,
		struct dce_rpc_hdr **rsp, int len)
{
	uint16_t req_id = priv->req_ids[(token-1) % ARRAY_SIZE(priv->req_ids)];
	struct omap_dce_txn *txn = &txns[req_id % ARRAY_SIZE(txns)];
	int ret;

	if (txn->priv != priv) {
		dev_err(device, "not my txn\n");
		return -EINVAL;
	}

	ret = wait_event_interruptible(wq, (txn->rsp));
	if (ret) {
		DBG("ret=%d", ret);
		return ret;
	}

	if (txn->len < len) {
		dev_err(device, "rsp too short: %d < %d\n", txn->len, len);
		ret = -EINVAL;
		goto fail;
	}

	*rsp = txn->rsp;

fail:
	/* clear out state: */
	memset(txn, 0, sizeof(*txn));

	return ret;
}

static void txn_cleanup(struct omap_dce_txn *txn)
{
	mutex_lock(&lock);

	kfree(txn->rsp);
	txn->rsp = NULL;

	txn->bo_count = 0;

	mutex_unlock(&lock);
}

static void rpcomplete(struct dce_rpc_hdr *rsp, int len)
{
	struct omap_dce_txn *txn = &txns[rsp->req_id % ARRAY_SIZE(txns)];

	if (!txn->priv) {
		/* we must of cleaned up already (killed process) */
		printk(KERN_ERR "dce: unexpected response.. killed process?\n");
		kfree(rsp);
		return;
	}

	txn_cleanup(txn);

	txn->len = len;
	txn->rsp = rsp;

	wake_up_all(&wq);
}

/* helpers for tracking engine instances and mapping engine handle to engine
 * instance:
 */

static uint32_t codec_register(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(priv->codecs); i++) {
		if (!priv->codecs[i].codec) {
			priv->codecs[i].codec_id = codec_id;
			priv->codecs[i].codec = codec;
			return i+1;
		}
	}
	dev_err(device, "too many codecs\n");
	return 0;
}

static void codec_unregister(struct dce_file_priv *priv,
		uint32_t codec_handle)
{
	//codec_unlockbuf(priv, codec_handle, 0);
	priv->codecs[codec_handle-1].codec = 0;
	priv->codecs[codec_handle-1].codec_id = 0;
}

static bool codec_valid(struct dce_file_priv *priv, uint32_t codec_handle)
{
	return (codec_handle > 0) &&
			(codec_handle <= ARRAY_SIZE(priv->codecs)) &&
			(priv->codecs[codec_handle-1].codec);
}

static int codec_get(struct dce_file_priv *priv, uint32_t codec_handle,
		uint32_t *codec, uint32_t *codec_id)
{
	if (!codec_valid(priv, codec_handle))
		return -EINVAL;
	*codec    = priv->codecs[codec_handle-1].codec;
	*codec_id = priv->codecs[codec_handle-1].codec_id;
	return 0;
}

static uint32_t engine_register(struct dce_file_priv *priv, uint32_t engine)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(priv->engines); i++) {
		if (!priv->engines[i].engine) {
			priv->engines[i].engine = engine;
			return i+1;
		}
	}
	dev_err(device, "too many engines\n");
	return 0;
}

static void engine_unregister(struct dce_file_priv *priv, uint32_t eng_handle)
{
	priv->engines[eng_handle-1].engine = 0;
}

static bool engine_valid(struct dce_file_priv *priv, uint32_t eng_handle)
{
	return (eng_handle > 0) &&
			(eng_handle <= ARRAY_SIZE(priv->engines)) &&
			(priv->engines[eng_handle-1].engine);
}

static int engine_get(struct dce_file_priv *priv, uint32_t eng_handle,
		uint32_t *engine)
{
	if (!engine_valid(priv, eng_handle))
		return -EINVAL;
	*engine = priv->engines[eng_handle-1].engine;
	return 0;
}

static int engine_close(struct dce_file_priv *priv, uint32_t engine);
static int codec_delete(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id);

static int ioctl_engine_open(struct dce_file_priv *priv, struct omap_dce_engine_open *arg)
{
	struct dce_rpc_engine_open_rsp *rsp;
	int ret;

	DBG("ioctl_engine_open");
	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		struct dce_rpc_engine_open_req req = {
				.hdr = MKHDR(ENGINE_OPEN),
		};
		strncpy(req.name, arg->name, sizeof(req.name));
		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
		if (ret)
			return ret;
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	arg->eng_handle = engine_register(priv, rsp->engine);
	arg->error_code = rsp->error_code;

	if (!engine_valid(priv, arg->eng_handle)) {
		engine_close(priv, rsp->engine);
		ret = -ENOMEM;
	}

	kfree(rsp);

	return ret;
}

static int engine_close(struct dce_file_priv *priv, uint32_t engine)
{
	struct dce_rpc_engine_close_req req = {
			.hdr = MKHDR(ENGINE_CLOSE),
			.engine = engine,
	};
	return rpsend(priv, NULL, hdr(&req), sizeof(req));
}

static int ioctl_engine_close(struct dce_file_priv *priv, struct omap_dce_engine_close *arg)
{
	uint32_t engine;
	int ret;

	DBG("ioctl_engine_close");
	ret = engine_get(priv, arg->eng_handle, &engine);
	if (ret)
		return ret;

	engine_unregister(priv, arg->eng_handle);

	return engine_close(priv, engine);
}

static int ioctl_codec_create(struct dce_file_priv *priv, struct omap_dce_codec_create *arg)
{
	struct dce_rpc_codec_create_rsp *rsp;
	int ret;

	DBG("ioctl_codec_create");
	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		size_t len;
		ion_phys_addr_t paddr;
		struct dce_rpc_codec_create_req req = {
				.hdr = MKHDR(CODEC_CREATE),
				.codec_id = arg->codec_id,
		};

		strncpy(req.name, arg->name, sizeof(req.name));

		ret = engine_get(priv, arg->eng_handle, &req.engine);
		if (ret) {
			printk(KERN_ERR "ioctl_codec_create failed: engine_get return %d\n", ret);
			return ret;
		}

		ret = ion_handle_phys((struct ion_handle *)arg->sparams_bo, &paddr, &len);
		if (ret) {
			printk(KERN_ERR "ioctl_codec_create failed: ion_handle_phys return %d\n", ret);
			return ret;
		}
		req.sparams = paddr;

		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
		if (ret) {
			printk(KERN_ERR "ioctl_codec_create failed: rpsend return %d\n", ret);
			return ret;
		}
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret) {
		printk(KERN_ERR "ioctl_codec_create failed: rpwait return %d\n", ret);
		return ret;
	}

	arg->codec_handle = codec_register(priv, rsp->codec, arg->codec_id);

	if (!codec_valid(priv, arg->codec_handle)) {
		codec_delete(priv, rsp->codec, arg->codec_id);
		ret = -ENOMEM;
		printk(KERN_ERR "ioctl_codec_create failed: codec not valid\n");
	}

	kfree(rsp);

	return ret;
}

static int ioctl_codec_control(struct dce_file_priv *priv, struct omap_dce_codec_control *arg)
{
	struct dce_rpc_codec_control_rsp *rsp;
	int ret;

	DBG("ioctl_codec_control");
	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		size_t len;
		ion_phys_addr_t paddr;
		struct dce_rpc_codec_control_req req = {
				.hdr = MKHDR(CODEC_CONTROL),
				.cmd_id = arg->cmd_id,
		};

		ret = codec_get(priv, arg->codec_handle, &req.codec, &req.codec_id);
		if (ret)
			return ret;

		ret = ion_handle_phys((struct ion_handle *)arg->dparams_bo, &paddr, &len);
		if (ret)
			return ret;
		req.dparams = paddr;

		ret = ion_handle_phys((struct ion_handle *)arg->status_bo, &paddr, &len);
		if (ret)
			return ret;
		req.status = paddr;

		ret = rpsend(priv, &arg->token, hdr(&req), sizeof(req));
		if (ret)
			return ret;
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
	if (ret)
		return ret;

	arg->result = rsp->result;

	kfree(rsp);

	return 0;
}

struct viddec3_in_args {
	int32_t size;		/* struct size */
	int32_t num_bytes;
	int32_t input_id;
};
struct videnc2_in_args {
	int32_t size;
	int32_t input_id;
	int32_t control;
};

union xdm2_buf_size {
    struct {
        int32_t width;
        int32_t height;
    } tiled;
    int32_t bytes;
};
struct xdm2_single_buf_desc {
    uint32_t buf;
    int16_t  mem_type;	/* XXX should be XDM_MEMTYPE_BO */
    int16_t  usage_mode;
    union xdm2_buf_size buf_size;
    int32_t  accessMask;
};
struct xdm2_buf_desc {
    int32_t num_bufs;
    struct xdm2_single_buf_desc descs[16];
};

struct video2_buf_desc {
    int32_t num_planes;
    int32_t num_meta_planes;
    int32_t data_layout;
    struct xdm2_single_buf_desc plane_desc[3];
    struct xdm2_single_buf_desc metadata_plane_desc[3];
    /* rest of the struct isn't interesting to kernel.. if you are
     * curious look at IVIDEO2_BufDesc in ivideo.h in codec-engine
     */
    uint32_t data[30];
};

/* copy_from_user helper that also checks to avoid overrunning
 * the 'to' buffer and advances dst ptr
 */
static inline int cfu(void **top, uint64_t from, int n, void *end)
{
	void *to = *top;
	int ret;
	if ((to + n) >= end) {
		DBG("dst buffer overflow!");
		return -EFAULT;
	}
	ret = copy_from_user(to, (char __user *)(uintptr_t)from, n);
	*top = to + n;
	return ret;
}

static u32 dce_virt_to_phys(u32 arg)
{
	pmd_t *pmd;
	pte_t *ptep;

	pgd_t *pgd = pgd_offset(current->mm, arg);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pmd = pmd_offset((pud_t *)pgd, arg);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, arg);
	if (ptep && pte_present(*ptep))
		return (PAGE_MASK & *ptep) | (~PAGE_MASK & arg);

	return 0;
}

static inline void dump_buf_desc(struct xdm2_single_buf_desc *desc)
{
	if (desc->mem_type >= XDM_MEMTYPE_TILED8) {
		printk("buf: paddr: 0x%X, size: %dx%d, type: %s\n", desc->buf,
		    desc->buf_size.tiled.width, desc->buf_size.tiled.height,
		    desc->mem_type == XDM_MEMTYPE_TILED8 ? "XDM_MEMTYPE_TILED8" : "XDM_MEMTYPE_TILED16");
	} else {
		printk("buf: paddr: 0x%X, size: %d, type: RAW\n", desc->buf,
		    desc->buf_size.bytes);
	}
}

static inline int handle_single_buf_desc(
		struct dce_file_priv *priv, struct dce_rpc_hdr *req,
		struct xdm2_single_buf_desc *desc)
{
	int ret;
	size_t len;
	ion_phys_addr_t paddr;

DBG("handle_single_buf_desc: bu %08X", desc->buf);
	if( desc->buf >= 0xC0000000 ) {
		// buf is an ion handle...
		ret = ion_handle_phys((struct ion_handle *)desc->buf, &paddr, &len);
	} else {
		// buf is a vaddr...
		paddr = dce_virt_to_phys( desc->buf );
		ret = !paddr;
	}
DBG("handle_single_buf_desc: pa %08lX", paddr);
	if (ret)
		return ret;
	desc->buf = paddr;

	if (debug_dce)
		dump_buf_desc(desc);

	return ret;
}

static inline int handle_pvrfd_buf_desc(struct dce_file_priv *priv,
		struct xdm2_single_buf_desc *desc, ion_phys_addr_t paddr)
{
	desc->buf = paddr;
	if (paddr >= 0x60000000 && paddr <= 0x67FFFFFF) {
		desc->mem_type = XDM_MEMTYPE_TILED8;
	} else if (paddr >= 0x68000000 && paddr <= 0x6FFFFFFF) {
		desc->mem_type = XDM_MEMTYPE_TILED16;
	} else {
		pr_err("%s: paddr not handled: 0x%lX\n", __func__, paddr);
		return -1;
	}
	if (debug_dce)
		dump_buf_desc(desc);
	return 0;
}

static inline int handle_pvrfd_buf_descs(struct dce_file_priv *priv,
		struct xdm2_buf_desc *bufs)
{
	int ret = -1;
	int fd;
	int i;
	int nb_handles = 2;
	size_t unused;
	struct ion_client *pvr_ion_client;
	struct ion_handle *handles[2] = { NULL, NULL };
	ion_phys_addr_t paddr;


	fd = (int) bufs->descs[0].buf;
	omap_ion_fd_to_handles(fd, &pvr_ion_client, handles, &nb_handles);
	if ((nb_handles != 2) || !handles[0] || !handles[1]) {
		pr_err("%s: Dce can not export ION fd\n", __func__);
		return -1;
	}
	for (i = 0; i < 2; ++i) {
		if (!ion_phys(pvr_ion_client, handles[i], &paddr, &unused)) {
			ret = handle_pvrfd_buf_desc(priv, &bufs->descs[i], paddr);

			if (ret)
				return ret;
		}
	}

	return ret;
}

static inline int handle_buf_desc(void **ptr, void *end,
		struct dce_file_priv *priv, struct dce_rpc_hdr *req, uint64_t usr,
		uint8_t *len)
{
	struct xdm2_buf_desc *bufs = *ptr;
	int i;
	int ret;

	/* read num_bufs field: */
	ret = cfu(ptr, usr, 4, end);
	if (ret)
		return ret;

	/* read rest of structure: */
	ret = cfu(ptr, usr+4, bufs->num_bufs * sizeof(bufs->descs[0]), end);
	if (ret)
		return ret;

	*len = (4 + bufs->num_bufs * sizeof(bufs->descs[0])) / 4;

	/* handle buffer mapping.. */

	if (bufs->num_bufs == 2 && bufs->descs[0].mem_type == XDM_MEMTYPE_PVRFD) {
		ret = handle_pvrfd_buf_descs(priv, bufs);

		if (ret)
			return ret;
	} else {
		for (i = 0; i < bufs->num_bufs; i++) {
			ret = handle_single_buf_desc(priv, req, &bufs->descs[i]);

			if (ret)
				return ret;
		}
	}

	return 0;
}

/*
 *   VIDDEC3_process			VIDENC2_process
 *   VIDDEC3_InArgs *inArgs		VIDENC2_InArgs *inArgs
 *   XDM2_BufDesc *outBufs		XDM2_BufDesc *outBufs
 *   XDM2_BufDesc *inBufs		VIDEO2_BufDesc *inBufs
 */

static inline int handle_videnc2(void **ptr, void *end,
		struct dce_file_priv *priv,
		struct dce_rpc_codec_process_req *req,
		struct omap_dce_codec_process *arg)
{
	WARN_ON(1); // XXX not implemented
	//	codec_lockbuf(priv, arg->codec_handle, in_args->input_id, y, uv);
	return -EFAULT;
}

static inline int handle_viddec3(void **ptr, void *end,
		struct dce_file_priv *priv,
		struct dce_rpc_codec_process_req *req,
		struct omap_dce_codec_process *arg)
{
	struct viddec3_in_args *in_args = *ptr;
	int ret;

	/* handle in_args: */
	ret = cfu(ptr, arg->in_args, sizeof(*in_args), end);
	if (ret)
		return ret;

	if (in_args->size > sizeof(*in_args)) {
		int sz = in_args->size - sizeof(*in_args);
		/* this param can be variable length */
		ret = cfu(ptr, arg->in_args + sizeof(*in_args), sz, end);
		if (ret)
			return ret;
		/* in case the extra part size is not multiple of 4 */
		*ptr += round_up(sz, 4) - sz;
	}

	req->in_args_len = round_up(in_args->size, 4) / 4;

	/* handle out_bufs: */
	ret = handle_buf_desc(ptr, end, priv, hdr(req), arg->out_bufs,
			&req->out_bufs_len);
	if (ret)
		return ret;

	/* handle in_bufs: */
	ret = handle_buf_desc(ptr, end, priv, hdr(req), arg->in_bufs,
			&req->in_bufs_len);
	if (ret)
		return ret;

	return 0;
}

#define RPMSG_BUF_SIZE		(512)  // ugg, would be nice not to hard-code..

static int ioctl_codec_process(struct dce_file_priv *priv, struct omap_dce_codec_process *arg)
{
	struct dce_rpc_codec_process_rsp *rsp;
	int ret;
	long tprocess = mark(NULL);

	DBG("ioctl_codec_process");
	/* if we are not re-starting a syscall, send req */
	if (!arg->token) {
		size_t len;
		ion_phys_addr_t paddr;
		long tsend = mark(NULL);
		/* worst-case size allocation.. */
		struct dce_rpc_codec_process_req *req = kzalloc(RPMSG_BUF_SIZE, GFP_KERNEL);
		void *ptr = &req->data[0];
		void *end = ((void *)req) + RPMSG_BUF_SIZE;

		req->hdr = MKHDR(CODEC_PROCESS);

		ret = codec_get(priv, arg->codec_handle, &req->codec, &req->codec_id);
		if (ret)
			goto rpsend_out;

		ret = ion_handle_phys((struct ion_handle *)arg->out_args_bo, &paddr, &len);
		if (ret)
			goto rpsend_out;
		req->out_args = paddr;

		/* the remainder of the req varies depending on codec family */
		switch (req->codec_id) {
		case OMAP_DCE_VIDENC2:
			ret = handle_videnc2(&ptr, end, priv, req, arg);
			break;
		case OMAP_DCE_VIDDEC3:
			ret = handle_viddec3(&ptr, end, priv, req, arg);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret)
			goto rpsend_out;

		ret = rpsend(priv, &arg->token, hdr(req), ptr - (void *)req);
DBG("send in: %ld us", mark(&tsend));
rpsend_out:
		kfree(req);
		if (ret)
			return ret;
	}

	/* then wait for reply, which is interruptible */
	ret = rpwait(priv, arg->token, hdr(&rsp), sizeof(*rsp));
DBG("process in: %ld us", mark(&tprocess));
	if (ret)
		return ret;
#if 0
	for (i = 0; i < rsp->count; i++) {
		codec_unlockbuf(priv, arg->codec_handle, rsp->freebuf_ids[i]);
	}
#endif

	arg->result = rsp->result;

	kfree(rsp);

	return 0;
}

static int ioctl_codec_delete(struct dce_file_priv *priv, struct omap_dce_codec_delete *arg)
{
	uint32_t codec, codec_id;
	int ret;

	DBG("ioctl_codec_delete");
	ret = codec_get(priv, arg->codec_handle, &codec, &codec_id);
	if (ret)
		return ret;

	codec_unregister(priv, arg->codec_handle);

	return codec_delete(priv, codec, codec_id);
}

static int codec_delete(struct dce_file_priv *priv, uint32_t codec,
		enum omap_dce_codec codec_id)
{
	struct dce_rpc_codec_delete_req req = {
			.hdr = MKHDR(CODEC_DELETE),
			.codec_id = codec_id,
			.codec = codec,
	};
	return rpsend(priv, NULL, hdr(&req), sizeof(req));
}

static int dce_open(struct inode *ip, struct file *filp)
{
	struct dce_file_priv *priv;
DBG("dce_open");
	if( !rpdev ) {
		return EINVAL;
	}
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	priv->file = filp;
	filp->private_data = priv;

	return 0;
}

static int dce_release(struct inode *ip, struct file *filp)
{
	struct dce_file_priv *priv = filp->private_data;
	int i;
DBG("dce_release");

	/* cleanup any remaining codecs and engines on behalf of the process,
	 * in case the process crashed or didn't clean up properly for itself:
	 */

	for (i = 0; i < ARRAY_SIZE(priv->codecs); i++) {
		uint32_t codec = priv->codecs[i].codec;
		if (codec) {
			enum omap_dce_codec codec_id = priv->codecs[i].codec_id;
			codec_unregister(priv, i+1);
			codec_delete(priv, codec, codec_id);
		}
	}

	for (i = 0; i < ARRAY_SIZE(priv->engines); i++) {
		uint32_t engine = priv->engines[i].engine;
		if (engine) {
			engine_unregister(priv, i+1);
			engine_close(priv, engine);
		}
	}

	for (i = 0; i < ARRAY_SIZE(txns); i++) {
		if (txns[i].priv == priv) {
			txn_cleanup(&txns[i]);
			memset(&txns[i], 0, sizeof(txns[i]));
		}
	}

	kfree(priv);

	return 0;
}

static long dce_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 ret = -EINVAL;
	struct dce_file_priv *priv = filp->private_data;
	switch (cmd) {
		case OMAP_DCE_ENGINE_OPEN:
		{
			struct omap_dce_engine_open p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_engine_open)))
				return -EINVAL;
			if ((ret = ioctl_engine_open(priv, &p)))
				return ret;
			if (copy_to_user((void __user *)arg, &p, sizeof(struct omap_dce_engine_open)))
				return -EINVAL;
			break;
		}
		case OMAP_DCE_ENGINE_CLOSE:
		{
			struct omap_dce_engine_close p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_engine_close)))
				return -EINVAL;
			if ((ret = ioctl_engine_close(priv, &p)))
				return ret;
			break;
		}
		case OMAP_DCE_CODEC_CREATE:
		{
			struct omap_dce_codec_create p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_codec_create)))
				return -EINVAL;
			if ((ret = ioctl_codec_create(priv, &p)))
				return ret;
			if (copy_to_user((void __user *)arg, &p, sizeof(struct omap_dce_codec_create)))
				return -EINVAL;
			break;
		}
		case OMAP_DCE_CODEC_CONTROL:
		{
			struct omap_dce_codec_control p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_codec_control)))
				return -EINVAL;
			if ((ret = ioctl_codec_control(priv, &p)))
				return ret;
			if (copy_to_user((void __user *)arg, &p, sizeof(struct omap_dce_codec_control)))
				return -EINVAL;
			break;
		}
		case OMAP_DCE_CODEC_PROCESS:
		{
			struct omap_dce_codec_process p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_codec_process)))
				return -EINVAL;
			if ((ret = ioctl_codec_process(priv, &p)))
				return ret;
			if (copy_to_user((void __user *)arg, &p, sizeof(struct omap_dce_codec_process)))
				return -EINVAL;
			break;
		}
		case OMAP_DCE_CODEC_DELETE:
		{
			struct omap_dce_codec_delete p;

			if (copy_from_user(&p, (void __user *)arg, sizeof(struct omap_dce_codec_delete)))
				return -EINVAL;
			if ((ret = ioctl_codec_delete(priv, &p)))
				return ret;
			break;
		}
	}
	return ret;
}

/*
 * RPMSG API:
 */

static int rpmsg_probe(struct rpmsg_channel *_rpdev)
{
	DBG("dce: rpmsg_probe");
	rpdev = _rpdev;
	return 0;
}

static void __devexit rpmsg_remove(struct rpmsg_channel *_rpdev)
{
	DBG("dce: rpmsg_remove");
	rpdev = NULL;
}

static void rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
		int len, void *priv, u32 src)
{
	void *data2;
	DBG("dce: rpmsg_cb: len=%d, src=%d", len, src);
	/* note: we have to copy the data, because the ptr is no more valid
	 * once this fxn returns, and it could take a while for the requesting
	 * thread to pick up the data.. maybe there is a more clever way to
	 * handle this..
	 */
	data2 = kzalloc(len, GFP_KERNEL);
	memcpy(data2, data, len);
	rpcomplete(data2, len);
}

static struct rpmsg_device_id rpmsg_id_table[] = {
		{ .name = "rpmsg-dce" },
		{ },
};

static struct rpmsg_driver rpmsg_driver = {
		.drv.name	= KBUILD_MODNAME,
		.drv.owner	= THIS_MODULE,
		.id_table	= rpmsg_id_table,
		.probe		= rpmsg_probe,
		.callback	= rpmsg_cb,
		.remove		= __devexit_p(rpmsg_remove),
};

static const struct file_operations dce_fops = {
	.open		= dce_open,
	.unlocked_ioctl	= dce_ioctl,
	.release	= dce_release,
};

static struct platform_driver dce_driver_ldm = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "dce",
	},
};

static int __init omap_dce_init(void)
{
	dev_t dev = 0;
	int r = -EINVAL;

	DBG("omap_dce_init");

	dce_device = kmalloc(sizeof(struct dce_device), GFP_KERNEL);
	if (!dce_device) {
		r = -ENOMEM;
		printk(KERN_ERR "can not allocate device structure\n");
		goto error;
	}
	memset(dce_device, 0, sizeof(struct dce_device));
	if (dce_major) {
		dev = MKDEV(dce_major, dce_minor);
		r = register_chrdev_region(dev, 1, "dce");
	} else {
		r = alloc_chrdev_region(&dev, dce_minor, 1, "dce");
		dce_major = MAJOR(dev);
	}

	cdev_init(&dce_device->cdev, &dce_fops);
	dce_device->cdev.owner = THIS_MODULE;
	dce_device->cdev.ops   = &dce_fops;

	r = cdev_add(&dce_device->cdev, dev, 1);
	if (r) {
		printk(KERN_ERR "cdev_add():failed\n");
		goto error;
	}

	dcedev_class = class_create(THIS_MODULE, "dce");

	if (IS_ERR(dcedev_class)) {
		printk(KERN_ERR "class_create():failed\n");
		goto error;
	}

	device = device_create(dcedev_class, NULL, dev, NULL, "dce");
	if (device == NULL)
		printk(KERN_ERR "device_create() fail\n");

	r = platform_driver_register(&dce_driver_ldm);
	

	r = register_rpmsg_driver(&rpmsg_driver);
error:
	if (r) {
		kfree(dce_device);
	}
	return r;
}

static void __exit omap_dce_exit(void)
{
	DBG("omap_dce_exit");
	unregister_rpmsg_driver(&rpmsg_driver);

	platform_driver_unregister(&dce_driver_ldm);
	cdev_del(&dce_device->cdev);
	kfree(dce_device);
	device_destroy(dcedev_class, MKDEV(dce_major, dce_minor));
	class_destroy(dcedev_class);
}

module_init(omap_dce_init);
module_exit(omap_dce_exit);

MODULE_AUTHOR("Rob Clark <rob.clark@linaro.org>");
MODULE_DESCRIPTION("OMAP DRM Video Decode/Encode");
MODULE_LICENSE("GPL v2");
