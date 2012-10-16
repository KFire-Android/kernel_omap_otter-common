/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
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

#include "omap_rpc_internal.h"

static struct class *omaprpc_class;
static dev_t omaprpc_dev;

/* store all remote rpc connection services (usually one per remoteproc) */

/* could also use DEFINE_IDR(omaprpc_services); */
static struct idr omaprpc_services = IDR_INIT(omaprpc_services);

/* could also use DEFINE_SPINLOCK(omaprpc_services_lock); */
static spinlock_t omaprpc_services_lock =
__SPIN_LOCK_UNLOCKED(omaprpc_services_lock);

/* could also use LIST_HEAD(omaprpc_services_list); */
static struct list_head omaprpc_services_list =
LIST_HEAD_INIT(omaprpc_services_list);

static struct timeval start_time;
static struct timeval end_time;
static long usec_elapsed;
unsigned int omaprpc_debug;
EXPORT_SYMBOL(omaprpc_debug);
MODULE_PARM_DESC(debug, "Used to enable debug messages");
module_param_named(debug, omaprpc_debug, int, 0600);

static void omaprpc_print_msg(struct omaprpc_instance_t *rpc,
			      char *prefix, char buffer[512])
{
	u32 sp = 0, p = 0;
	struct omaprpc_msg_header_t *hdr =
	    (struct omaprpc_msg_header_t *)buffer;
	struct omaprpc_instance_handle_t *hdl = NULL;
	struct omaprpc_instance_info_t *info = NULL;
	struct omaprpc_packet_t *packet = NULL;
	struct omaprpc_parameter_t *param = NULL;
	OMAPRPC_PRINT(OMAPRPC_ZONE_VERBOSE, rpc->rpcserv->dev,
		      "%s HDR: type %d flags: %d len: %d\n",
		      prefix, hdr->msg_type, hdr->msg_flags, hdr->msg_len);
	switch (hdr->msg_type) {
	case OMAPRPC_MSG_INSTANCE_CREATED:
	case OMAPRPC_MSG_INSTANCE_DESTROYED:
		hdl = OMAPRPC_PAYLOAD(buffer, omaprpc_instance_handle_t);
		OMAPRPC_PRINT(OMAPRPC_ZONE_VERBOSE,
			      rpc->rpcserv->dev,
			      "%s endpoint:%d status:%d\n",
			      prefix, hdl->endpoint_address, hdl->status);
		break;
	case OMAPRPC_MSG_INSTANCE_INFO:
		info = OMAPRPC_PAYLOAD(buffer, omaprpc_instance_info_t);
		OMAPRPC_PRINT(OMAPRPC_ZONE_VERBOSE,
			      rpc->rpcserv->dev,
			      "%s (info not yet implemented)\n", prefix);
		break;
	case OMAPRPC_MSG_CALL_FUNCTION:
		packet = OMAPRPC_PAYLOAD(buffer, omaprpc_packet_t);
		OMAPRPC_PRINT(OMAPRPC_ZONE_VERBOSE,
			      rpc->rpcserv->dev,
			      "%s PACKET: desc:%04x msg_id:%04x pool_id:%04x"
			      " job_id:%04x func:0x%08x result:%d size:%u\n",
			      prefix,
			      packet->desc,
			      packet->msg_id,
			      packet->pool_id,
			      packet->job_id,
			      packet->fxn_idx,
			      packet->result, packet->data_size);
		sp = sizeof(struct omaprpc_parameter_t);
		param = (struct omaprpc_parameter_t *)packet->data;
		for (p = 0; p < (packet->data_size / sp); p++) {
			OMAPRPC_PRINT(OMAPRPC_ZONE_VERBOSE,
				      rpc->rpcserv->dev,
				      "%s PARAM[%u] size:%zu data:%zu (0x%08x)",
				      prefix, p,
				      param[p].size,
				      param[p].data, param[p].data);
		}
		break;
	default:
		break;
	}
}

static void omaprpc_fxn_del(struct omaprpc_instance_t *rpc)
{
	/* Free any outstanding function calls */
	if (!list_empty(&rpc->fxn_list)) {
		struct omaprpc_call_function_list_t *pos, *n;

		mutex_lock(&rpc->lock);
		list_for_each_entry_safe(pos, n, &rpc->fxn_list, list) {
			list_del(&pos->list);
			kfree(pos->function);
			kfree(pos);
		}
		mutex_unlock(&rpc->lock);
	}
}

static struct omaprpc_call_function_t *omaprpc_fxn_get(struct omaprpc_instance_t
						       *rpc, u16 msgId)
{
	struct omaprpc_call_function_t *function = NULL;
	struct omaprpc_call_function_list_t *pos, *n;

	mutex_lock(&rpc->lock);
	list_for_each_entry_safe(pos, n, &rpc->fxn_list, list) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "Looking for msg %u, found msg %u\n",
			      msgId, pos->msgId);
		if (pos->msgId == msgId) {
			function = pos->function;
			list_del(&pos->list);
			kfree(pos);
			break;
		}
	}
	mutex_unlock(&rpc->lock);
	return function;
}

static int omaprpc_fxn_add(struct omaprpc_instance_t *rpc,
			   struct omaprpc_call_function_t *function, u16 msgId)
{
	struct omaprpc_call_function_list_t *fxn = NULL;
	fxn = (struct omaprpc_call_function_list_t *)
	    kmalloc(sizeof(struct omaprpc_call_function_list_t), GFP_KERNEL);
	if (fxn) {
		fxn->function = function;
		fxn->msgId = msgId;
		mutex_lock(&rpc->lock);
		list_add(&fxn->list, &rpc->fxn_list);
		mutex_unlock(&rpc->lock);
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "Added msg id %u to list", msgId);
	} else {
		OMAPRPC_ERR(rpc->rpcserv->dev,
			    "Failed to add function %p to list with id %d\n",
			    function, msgId);
		return -ENOMEM;
	}
	return 0;
}

/* This is the callback from the remote core to this side */
static void omaprpc_cb(struct rpmsg_channel *rpdev,
		       void *data, int len, void *priv, u32 src)
{
	struct omaprpc_msg_header_t *hdr = data;
	struct omaprpc_instance_t *rpc = priv;
	struct omaprpc_instance_handle_t *hdl;
	struct sk_buff *skb;
	char *buf = (char *)data;
	char *skbdata;
	u32 expected = 0;

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "OMAPRPC: <== incoming msg src %d len %d msg_type %d msg_len %d\n",
		      src, len, hdr->msg_type, hdr->msg_len);

	if (omaprpc_debug & OMAPRPC_ZONE_VERBOSE) {
		print_hex_dump(KERN_DEBUG, "OMAPRPC: RX: ",
			       DUMP_PREFIX_NONE, 16, 1, data, len, true);
		omaprpc_print_msg(rpc, "RX:", buf);
	}

	expected = sizeof(struct omaprpc_msg_header_t);
	switch (hdr->msg_type) {
	case OMAPRPC_MSG_INSTANCE_CREATED:
	case OMAPRPC_MSG_INSTANCE_DESTROYED:
		expected += sizeof(struct omaprpc_instance_handle_t);
		break;
	case OMAPRPC_MSG_INSTANCE_INFO:
		expected += sizeof(struct omaprpc_instance_info_t);
		break;
	}

	if (len < expected) {
		OMAPRPC_ERR(rpc->rpcserv->dev,
			    "OMAPRPC: truncated message detected! Was %u bytes long"
			    " expected %u\n", len, expected);
		rpc->state = OMAPRPC_STATE_FAULT;
		return;
	}

	switch (hdr->msg_type) {
	case OMAPRPC_MSG_INSTANCE_CREATED:
		hdl = OMAPRPC_PAYLOAD(buf, omaprpc_instance_handle_t);
		if (hdl->status) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "OMAPRPC: Failed to connect to remote core! "
				    "Status=%d\n", hdl->status);
			rpc->state = OMAPRPC_STATE_FAULT;
		} else {
			OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
				      "OMAPRPC: Created addr %d status %d\n",
				      hdl->endpoint_address, hdl->status);
			/* only save the address if it connected. */
			rpc->dst = hdl->endpoint_address;
			rpc->state = OMAPRPC_STATE_CONNECTED;
			/* default core */
			rpc->core = OMAPRPC_CORE_MCU1;
		}
		rpc->transisioning = 0;
		/* unblock the connect function */
		complete(&rpc->reply_arrived);
		break;
	case OMAPRPC_MSG_INSTANCE_DESTROYED:
		hdl = OMAPRPC_PAYLOAD(buf, omaprpc_instance_handle_t);
		if (hdr->msg_len < sizeof(*hdl)) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "OMAPRPC: disconnect message was incorrect size!\n");
			rpc->state = OMAPRPC_STATE_FAULT;
			break;
		}
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "OMAPRPC: endpoint %d disconnected!\n",
			      hdl->endpoint_address);
		rpc->state = OMAPRPC_STATE_DISCONNECTED;
		rpc->dst = 0;
		rpc->transisioning = 0;
		/* unblock the disconnect ioctl call */
		complete(&rpc->reply_arrived);
		break;
	case OMAPRPC_MSG_INSTANCE_INFO:
		break;
	case OMAPRPC_MSG_CALL_FUNCTION:
	case OMAPRPC_MSG_FUNCTION_RETURN:

		if (omaprpc_debug & OMAPRPC_ZONE_PERF) {
			do_gettimeofday(&end_time);
			usec_elapsed = (end_time.tv_sec - start_time.tv_sec) *
			    1000000 + end_time.tv_usec - start_time.tv_usec;
			OMAPRPC_PRINT(OMAPRPC_ZONE_PERF, rpc->rpcserv->dev,
				      "write to callback took %lu usec\n",
				      usec_elapsed);
		}

		skb = alloc_skb(hdr->msg_len, GFP_KERNEL);
		if (!skb) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "OMAPRPC: alloc_skb err: %u\n",
				    hdr->msg_len);
			break;
		}
		skbdata = skb_put(skb, hdr->msg_len);
		memcpy(skbdata, hdr->msg_data, hdr->msg_len);

		mutex_lock(&rpc->lock);

		if (omaprpc_debug & OMAPRPC_ZONE_PERF) {
			/* capture the time delay between callback and read */
			do_gettimeofday(&start_time);
		}

		skb_queue_tail(&rpc->queue, skb);
		mutex_unlock(&rpc->lock);
		/* wake up any blocking processes, waiting for new data */
		wake_up_interruptible(&rpc->readq);
		break;
	default:
		dev_warn(rpc->rpcserv->dev,
			 "OMAPRPC: unexpected msg type: %d\n", hdr->msg_type);
		break;
	}
}

static int omaprpc_connect(struct omaprpc_instance_t *rpc,
			   struct omaprpc_create_instance_t *connect)
{
	struct omaprpc_service_t *rpcserv = rpc->rpcserv;
	char kbuf[512];
	struct omaprpc_msg_header_t *hdr =
	    (struct omaprpc_msg_header_t *)&kbuf[0];
	int ret = 0;
	u32 len = 0;

	/* Return "is connected" if connected */
	if (rpc->state == OMAPRPC_STATE_CONNECTED) {
		dev_dbg(rpcserv->dev, "OMAPRPC: endpoint already connected\n");
		return -EISCONN;
	}

	/* Initialize the Connection Request Message */
	hdr->msg_type = OMAPRPC_MSG_CREATE_INSTANCE;
	hdr->msg_len = sizeof(struct omaprpc_create_instance_t);
	memcpy(hdr->msg_data, connect, hdr->msg_len);
	len = sizeof(struct omaprpc_msg_header_t) + hdr->msg_len;

	/* Initialize a completion object */
	init_completion(&rpc->reply_arrived);

	/* indicate that we are waiting for a completion */
	rpc->transisioning = 1;

	/*
	 * send a conn req to the remote RPC connection service. use
	 * the new local address that was just allocated by ->open
	 */
	ret = rpmsg_send_offchannel(rpcserv->rpdev,
				    rpc->ept->addr,
				    rpcserv->rpdev->dst, (char *)kbuf, len);
	if (ret > 0) {
		OMAPRPC_ERR(rpcserv->dev,
			    "OMAPRPC: rpmsg_send failed: %d\n", ret);
		return ret;
	}

	/* wait until a connection reply arrives or 5 seconds elapse */
	ret = wait_for_completion_interruptible_timeout(&rpc->reply_arrived,
							msecs_to_jiffies(5000));
	if (rpc->state == OMAPRPC_STATE_CONNECTED)
		return 0;

	if (rpc->state == OMAPRPC_STATE_FAULT)
		return -ENXIO;

	if (ret > 0) {
		OMAPRPC_ERR(rpcserv->dev,
			    "OMAPRPC: premature wakeup: %d\n", ret);
		return -EIO;
	}

	return -ETIMEDOUT;
}

static long omaprpc_ioctl(struct file *filp,
			  unsigned int cmd, unsigned long arg)
{
	struct omaprpc_instance_t *rpc = filp->private_data;
	struct omaprpc_service_t *rpcserv = rpc->rpcserv;
	struct omaprpc_create_instance_t connect;
	int ret = 0;

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: %s: cmd %d, arg 0x%lx\n", __func__, cmd, arg);

	/*
	 * if the magic was not present, tell the caller
	 *that we are not a typewritter[sic]!
	 */
	if (_IOC_TYPE(cmd) != OMAPRPC_IOC_MAGIC)
		return -ENOTTY;

	/*
	 * if the number of the command is larger than what
	 * we support, also tell the caller that we are not a typewriter[sic]!
	 */
	if (_IOC_NR(cmd) > OMAPRPC_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case OMAPRPC_IOC_CREATE:
		/* copy the connection buffer from the user */
		ret = copy_from_user(&connect, (char __user *)arg,
				     sizeof(connect));
		if (ret) {
			OMAPRPC_ERR(rpcserv->dev,
				    "OMAPRPC: %s: %d: copy_from_user fail: %d\n",
				    __func__, _IOC_NR(cmd), ret);
			ret = -EFAULT;
		} else {
			/* make sure user input is null terminated */
			connect.name[sizeof(connect.name) - 1] = '\0';
			/* connect to the remote core */
			ret = omaprpc_connect(rpc, &connect);
		}
		break;
	case OMAPRPC_IOC_DESTROY:
		break;
#if defined(OMAPRPC_USE_ION)
	case OMAPRPC_IOC_IONREGISTER:
		{
			struct ion_fd_data data;
			if (copy_from_user
			    (&data, (char __user *)arg, sizeof(data))) {
				ret = -EFAULT;
				OMAPRPC_ERR(rpcserv->dev,
					    "OMAPRPC: %s: %d: copy_from_user fail: %d\n",
					    __func__, _IOC_NR(cmd), ret);
			}
			data.handle = ion_import_fd(rpc->ion_client, data.fd);
			if (IS_ERR(data.handle))
				data.handle = NULL;
			if (copy_to_user
			    ((char __user *)arg, &data, sizeof(data))) {
				ret = -EFAULT;
				OMAPRPC_ERR(rpcserv->dev,
					    "OMAPRPC: %s: %d: copy_to_user fail: %d\n",
					    __func__, _IOC_NR(cmd), ret);
			}
			break;
		}
	case OMAPRPC_IOC_IONUNREGISTER:
		{
			struct ion_fd_data data;
			if (copy_from_user
			    (&data, (char __user *)arg, sizeof(data))) {
				ret = -EFAULT;
				OMAPRPC_ERR(rpcserv->dev,
					    "OMAPRPC: %s: %d: copy_from_user fail: %d\n",
					    __func__, _IOC_NR(cmd), ret);
			}
			ion_free(rpc->ion_client, data.handle);
			if (copy_to_user
			    ((char __user *)arg, &data, sizeof(data))) {
				ret = -EFAULT;
				OMAPRPC_ERR(rpcserv->dev,
					    "OMAPRPC: %s: %d: copy_to_user fail: %d\n",
					    __func__, _IOC_NR(cmd), ret);
			}
			break;
		}
#endif
	default:
		OMAPRPC_ERR(rpcserv->dev,
			    "OMAPRPC: unhandled ioctl cmd: %d\n", cmd);
		break;
	}

	return ret;
}

static int omaprpc_open(struct inode *inode, struct file *filp)
{
	struct omaprpc_service_t *rpcserv;
	struct omaprpc_instance_t *rpc;

	/* get the service pointer out of the inode */
	rpcserv = container_of(inode->i_cdev, struct omaprpc_service_t, cdev);

	/*
	 * Return EBUSY if we are down and if non-blocking or waiting for
	 * something
	 */
	if (rpcserv->state == OMAPRPC_SERVICE_STATE_DOWN)
		if ((filp->f_flags & O_NONBLOCK) ||
		    wait_for_completion_interruptible(&rpcserv->comp))
			return -EBUSY;

	/* Create a new instance */
	rpc = kzalloc(sizeof(*rpc), GFP_KERNEL);
	if (!rpc)
		return -ENOMEM;

	/* Initialize the instance mutex */
	mutex_init(&rpc->lock);

	/* Initialize the queue head for the socket buffers */
	skb_queue_head_init(&rpc->queue);

	/* Initialize the reading queue */
	init_waitqueue_head(&rpc->readq);

	/* Save the service pointer within the instance for later reference */
	rpc->rpcserv = rpcserv;
	rpc->state = OMAPRPC_STATE_DISCONNECTED;
	rpc->transisioning = 0;

	/* Initialize the current msg id */
	rpc->msgId = 0;

	/* Initialize the remember function call list */
	INIT_LIST_HEAD(&rpc->fxn_list);

#if defined(OMAPRPC_USE_DMABUF)
	INIT_LIST_HEAD(&rpc->dma_list);
#endif

	/*
	 * assign a new, unique, local address and
	 * associate the instance with it
	 */
	rpc->ept = rpmsg_create_ept(rpcserv->rpdev, omaprpc_cb, rpc,
				    RPMSG_ADDR_ANY);
	if (!rpc->ept) {
		OMAPRPC_ERR(rpcserv->dev, "OMAPRPC: create ept failed\n");
		kfree(rpc);
		return -ENOMEM;
	}
#if defined(OMAPRPC_USE_ION)
	/* get a handle to the ion client for RPC buffers */
	rpc->ion_client = ion_client_create(omap_ion_device,
					    (1 << ION_HEAP_TYPE_CARVEOUT) |
					    (1 << OMAP_ION_HEAP_TYPE_TILER),
					    "rpmsg-rpc");
#endif

	/* remember rpc in filp's private data */
	filp->private_data = rpc;

	/* Add the instance to the service's list */
	mutex_lock(&rpcserv->lock);
	list_add(&rpc->list, &rpcserv->instance_list);
	mutex_unlock(&rpcserv->lock);

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: local addr assigned: 0x%x\n", rpc->ept->addr);

	return 0;
}

static int omaprpc_release(struct inode *inode, struct file *filp)
{
	char kbuf[512];
	u32 len = 0;
	int ret = 0;
	struct omaprpc_instance_t *rpc = NULL;
	struct omaprpc_service_t *rpcserv = NULL;
	if (inode == NULL || filp == NULL)
		return 0;

	/* a conveinence pointer to the instane */
	rpc = filp->private_data;
	/* a conveinence pointer to service */
	rpcserv = rpc->rpcserv;
	if (rpc == NULL || rpcserv == NULL)
		return 0;

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "Releasing Instance %p, in state %d\n", rpc, rpc->state);
	/* if we are in a normal state */
	if (rpc->state != OMAPRPC_STATE_FAULT) {
		/* if we have connected already */
		if (rpc->state == OMAPRPC_STATE_CONNECTED) {
			struct omaprpc_msg_header_t *hdr =
			    (struct omaprpc_msg_header_t *)&kbuf[0];
			struct omaprpc_instance_handle_t *handle =
			    OMAPRPC_PAYLOAD(kbuf, omaprpc_instance_handle_t);

			/* Format a disconnect message */
			hdr->msg_type = OMAPRPC_MSG_DESTROY_INSTANCE;
			hdr->msg_len = sizeof(u32);
			/* end point address */
			handle->endpoint_address = rpc->dst;
			handle->status = 0;
			len =
			    sizeof(struct omaprpc_msg_header_t) + hdr->msg_len;

			OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
				      "OMAPRPC: Disconnecting from RPC service at %d\n",
				      rpc->dst);

			/* send the msg to the remote RPC connection service */
			ret = rpmsg_send_offchannel(rpcserv->rpdev,
						    rpc->ept->addr,
						    rpcserv->rpdev->dst,
						    (char *)kbuf, len);
			if (ret) {
				OMAPRPC_ERR(rpcserv->dev,
					    "OMAPRPC: rpmsg_send failed: %d\n",
					    ret);
				return ret;
			}

			/*
			 * TODO: Should we wait for a message to come back?
			 * For now, no.
			 */
			wait_for_completion_interruptible(&rpc->reply_arrived);

		}

		/* Destroy the local endpoint */
		if (rpc->ept) {
			rpmsg_destroy_ept(rpc->ept);
			rpc->ept = NULL;
		}
	}
#if defined(OMAPRPC_USE_ION)
	if (rpc->ion_client) {
		/* Destroy our local client to ion */
		ion_client_destroy(rpc->ion_client);
		rpc->ion_client = NULL;
	}
#endif
	/* Remove the function list */
	omaprpc_fxn_del(rpc);

	/* Remove the instance from the service's list */
	mutex_lock(&rpcserv->lock);
	list_del(&rpc->list);
	mutex_unlock(&rpcserv->lock);

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: Instance %p has been deleted!\n", rpc);

	if (list_empty(&rpcserv->instance_list)) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
			      "OMAPRPC: All instances have been removed!\n");
	}

	/* Delete the instance memory */
	filp->private_data = NULL;
	memset(rpc, 0xFE, sizeof(struct omaprpc_instance_t));
	kfree(rpc);
	rpc = NULL;
	return 0;
}

static ssize_t omaprpc_read(struct file *filp,
			    char __user *buf, size_t len, loff_t *offp)
{
	struct omaprpc_instance_t *rpc = filp->private_data;
	struct omaprpc_packet_t *packet = NULL;
	struct omaprpc_parameter_t *parameters = NULL;
	struct omaprpc_call_function_t *function = NULL;
	struct omaprpc_function_return_t returned;
	struct sk_buff *skb = NULL;
	int ret = 0;
	int use = sizeof(returned);

	/* too big */
	if (len > use) {
		ret = -EOVERFLOW;
		goto failure;
	}

	/* too small */
	if (len < use) {
		ret = -EINVAL;
		goto failure;
	}

	/* locked */
	if (mutex_lock_interruptible(&rpc->lock)) {
		ret = -ERESTARTSYS;
		goto failure;
	}

	/* error state */
	if (rpc->state == OMAPRPC_STATE_FAULT) {
		mutex_unlock(&rpc->lock);
		ret = -ENXIO;
		goto failure;
	}

	/* not connected to the remote side... */
	if (rpc->state != OMAPRPC_STATE_CONNECTED) {
		mutex_unlock(&rpc->lock);
		ret = -ENOTCONN;
		goto failure;
	}

	/* nothing to read ? */
	if (skb_queue_empty(&rpc->queue)) {
		mutex_unlock(&rpc->lock);
		/* non-blocking requested ? return now */
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto failure;
		}
		/* otherwise block, and wait for data */
		if (wait_event_interruptible(rpc->readq,
					     (!skb_queue_empty(&rpc->queue) ||
					      rpc->state ==
					      OMAPRPC_STATE_FAULT))) {
			ret = -ERESTARTSYS;
			goto failure;
		}
		/* re-grab the lock */
		if (mutex_lock_interruptible(&rpc->lock)) {
			ret = -ERESTARTSYS;
			goto failure;
		}
	}

	/* a fault happened while we waited. */
	if (rpc->state == OMAPRPC_STATE_FAULT) {
		mutex_unlock(&rpc->lock);
		ret = -ENXIO;
		goto failure;
	}

	/* pull the buffer out of the queue */
	skb = skb_dequeue(&rpc->queue);
	if (!skb) {
		mutex_unlock(&rpc->lock);
		OMAPRPC_ERR(rpc->rpcserv->dev,
			    "OMAPRPC: skb was NULL when dequeued, "
			    "possible race condition!\n");
		ret = -EIO;
		goto failure;
	}

	if (omaprpc_debug & OMAPRPC_ZONE_PERF) {
		do_gettimeofday(&end_time);
		usec_elapsed = (end_time.tv_sec - start_time.tv_sec) *
		    1000000 + end_time.tv_usec - start_time.tv_usec;
		OMAPRPC_PRINT(OMAPRPC_ZONE_PERF, rpc->rpcserv->dev,
			      "callback to read took %lu usec\n", usec_elapsed);
	}

	/* unlock the instances */
	mutex_unlock(&rpc->lock);

	packet = (struct omaprpc_packet_t *)skb->data;
	parameters = (struct omaprpc_parameter_t *)packet->data;

	/* pull the function memory from the list */
	function = omaprpc_fxn_get(rpc, packet->msg_id);
	if (function) {
		if (function->num_translations > 0) {
			/*
			 * Untranslate the PA pointers back
			 * to the ARM ION handles
			 */
			ret = omaprpc_xlate_buffers(rpc,
						    function,
						    OMAPRPC_RPA_TO_UVA);
			if (ret < 0)
				goto failure;
		}
	}
	returned.func_index = OMAPRPC_FXN_MASK(packet->fxn_idx);
	returned.status = packet->result;

	/* copy the kernel buffer to the user side */
	if (copy_to_user(buf, &returned, use)) {
		OMAPRPC_ERR(rpc->rpcserv->dev,
			    "OMAPRPC: %s: copy_to_user fail\n", __func__);
		ret = -EFAULT;
	} else {
		ret = use;
	}
failure:
	kfree(function);
	kfree_skb(skb);
	return ret;
}

static ssize_t omaprpc_write(struct file *filp,
			     const char __user *ubuf,
			     size_t len, loff_t *offp)
{
	struct omaprpc_instance_t *rpc = filp->private_data;
	struct omaprpc_service_t *rpcserv = rpc->rpcserv;
	struct omaprpc_msg_header_t *hdr = NULL;
	struct omaprpc_call_function_t *function = NULL;
	struct omaprpc_packet_t *packet = NULL;
	struct omaprpc_parameter_t *parameters = NULL;
	char kbuf[512];
	int use = 0, ret = 0, param = 0;

	/* incorrect parameter */
	if (len < sizeof(struct omaprpc_call_function_t)) {
		ret = -ENOTSUPP;
		goto failure;
	}

	/* over OMAPRPC_MAX_TRANSLATIONS! too many! */
	if (len > (sizeof(struct omaprpc_call_function_t) +
		   OMAPRPC_MAX_TRANSLATIONS *
		   sizeof(struct omaprpc_param_translation_t))) {
		ret = -ENOTSUPP;
		goto failure;
	}

	/* check the state of the driver */
	if (rpc->state != OMAPRPC_STATE_CONNECTED) {
		ret = -ENOTCONN;
		goto failure;
	}

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: Allocating local function call copy for %u bytes\n",
		      len);

	function = kzalloc(len, GFP_KERNEL);
	if (function == NULL) {
		/* could not allocate enough memory to cover the transaction */
		ret = -ENOMEM;
		goto failure;
	}

	/* copy the user packet to out memory */
	if (copy_from_user(function, ubuf, len)) {
		ret = -EMSGSIZE;
		goto failure;
	}

	/* increment the message ID and wrap if needed */
	rpc->msgId = (rpc->msgId + 1) & 0xFFFF;

	memset(kbuf, 0, sizeof(kbuf));
	hdr = (struct omaprpc_msg_header_t *)kbuf;
	hdr->msg_type = OMAPRPC_MSG_CALL_FUNCTION;
	hdr->msg_flags = 0;
	hdr->msg_len = sizeof(struct omaprpc_packet_t);
	packet = OMAPRPC_PAYLOAD(kbuf, omaprpc_packet_t);
	packet->desc = OMAPRPC_DESC_EXEC_SYNC;
	packet->msg_id = rpc->msgId;
	packet->pool_id = OMAPRPC_POOLID_DEFAULT;
	packet->job_id = OMAPRPC_JOBID_DISCRETE;
	packet->fxn_idx = OMAPRPC_SET_FXN_IDX(function->func_index);
	packet->result = 0;
	packet->data_size = sizeof(struct omaprpc_parameter_t) *
	    function->num_params;

	/* compute the parameter pointer changes last since this will cause the
	   cache operations */
	parameters = (struct omaprpc_parameter_t *)packet->data;
	for (param = 0; param < function->num_params; param++) {
		parameters[param].size = function->params[param].size;
		if (function->params[param].type == OMAPRPC_PARAM_TYPE_PTR) {
			/* internally the buffer translations takes care of the
			   offsets */
			void *reserved =
			    (void *)function->params[param].reserved;
			parameters[param].data =
			    (size_t) omaprpc_buffer_lookup(rpc,
							   rpc->core,
							   (virt_addr_t)
							   function->
							   params[param].data,
							   (virt_addr_t)
							   function->
							   params[param].base,
							   reserved);
		} else if (function->params[param].type ==
			   OMAPRPC_PARAM_TYPE_ATOMIC) {
			parameters[param].data = function->params[param].data;
		} else {
			ret = -ENOTSUPP;
			goto failure;
		}
	}

	/* Compute the size of the RPMSG packet */
	use = sizeof(*hdr) + hdr->msg_len + packet->data_size;

	/* failed to provide the translation data */
	if (function->num_translations > 0 &&
	    len < (sizeof(struct omaprpc_call_function_t) +
		   (function->num_translations *
		    sizeof(struct omaprpc_param_translation_t)))) {
		ret = -ENXIO;
		goto failure;
	}

	/* If there are pointers to translate for the user, do so now */
	if (function->num_translations > 0) {
		/* alter our copy of function and the user's parameters so
		   that we can send to remote cores */
		ret = omaprpc_xlate_buffers(rpc, function, OMAPRPC_UVA_TO_RPA);
		if (ret < 0) {
			OMAPRPC_ERR(rpcserv->dev,
				    "OMAPRPC: ERROR: Failed to translate all "
				    "pointers for remote core!\n");
			goto failure;
		}
	}

	/* save the function data */
	ret = omaprpc_fxn_add(rpc, function, rpc->msgId);
	if (ret < 0) {
		/* unwind */
		omaprpc_xlate_buffers(rpc, function, OMAPRPC_RPA_TO_UVA);
		goto failure;
	}

	/* dump the packet for debugging */
	if (omaprpc_debug & OMAPRPC_ZONE_VERBOSE) {
		print_hex_dump(KERN_DEBUG, "OMAPRPC: TX: ",
			       DUMP_PREFIX_NONE, 16, 1, kbuf, use, true);
		omaprpc_print_msg(rpc, "TX:", kbuf);
	}

	if (omaprpc_debug & OMAPRPC_ZONE_PERF) {
		/* capture the time delay between write and callback */
		do_gettimeofday(&start_time);
	}

	/* Send the msg */
	ret = rpmsg_send_offchannel(rpcserv->rpdev,
				    rpc->ept->addr, rpc->dst, kbuf, use);
	if (ret) {
		OMAPRPC_ERR(rpcserv->dev,
			    "OMAPRPC: rpmsg_send failed: %d\n", ret);
		/* remove the function data that we just saved */
		omaprpc_fxn_get(rpc, rpc->msgId);
		/* unwind */
		omaprpc_xlate_buffers(rpc, function, OMAPRPC_RPA_TO_UVA);
		goto failure;
	}
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: ==> Sent msg to remote endpoint %u\n",
		      rpc->dst);
failure:
	if (ret >= 0)
		ret = len;
	else
		kfree(function);

	/* return the length of the data written to us */
	return ret;
}

static unsigned int omaprpc_poll(struct file *filp,
				 struct poll_table_struct *wait)
{
	struct omaprpc_instance_t *rpc = filp->private_data;
	unsigned int mask = 0;

	/* grab the mutex so we can check the queue */
	if (mutex_lock_interruptible(&rpc->lock))
		return -ERESTARTSYS;

	/* Wait for items to enter the queue */
	poll_wait(filp, &rpc->readq, wait);
	if (rpc->state == OMAPRPC_STATE_FAULT) {
		mutex_unlock(&rpc->lock);
		return -ENXIO;	/* The remote service died somehow */
	}

	/* if the queue is not empty set the poll bit correctly */
	if (!skb_queue_empty(&rpc->queue))
		mask |= (POLLIN | POLLRDNORM);

	/* @TODO: implement missing rpmsg virtio functionality here */
	if (true)
		mask |= POLLOUT | POLLWRNORM;

	mutex_unlock(&rpc->lock);

	return mask;
}

static const struct file_operations omaprpc_fops = {
	.owner = THIS_MODULE,
	.open = omaprpc_open,
	.release = omaprpc_release,
	.unlocked_ioctl = omaprpc_ioctl,
	.read = omaprpc_read,
	.write = omaprpc_write,
	.poll = omaprpc_poll,
};

static int omaprpc_device_create(struct rpmsg_channel *rpdev)
{
	char kbuf[512];
	struct omaprpc_msg_header_t *hdr =
	    (struct omaprpc_msg_header_t *)&kbuf[0];
	int ret = 0;
	u32 len = 0;

	/* Initialize the Connection Request Message */
	hdr->msg_type = OMAPRPC_MSG_QUERY_CHAN_INFO;
	hdr->msg_len = 0;
	len = sizeof(struct omaprpc_msg_header_t);

	/* The device will be created during the reply */
	ret = rpmsg_send(rpdev, (char *)kbuf, len);
	if (ret) {
		dev_err(&rpdev->dev, "OMAPRPC: rpmsg_send failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int omaprpc_probe(struct rpmsg_channel *rpdev)
{
	int ret, major, minor;
	struct omaprpc_service_t *rpcserv = NULL, *tmp;

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, &rpdev->dev,
		      "OMAPRPC: Probing service with src %u dst %u\n",
		      rpdev->src, rpdev->dst);

again:				/* SMP systems could race device probes */

	/* allocate the memory for an integer ID */
	if (!idr_pre_get(&omaprpc_services, GFP_KERNEL)) {
		OMAPRPC_ERR(&rpdev->dev, "OMAPRPC: idr_pre_get failed\n");
		return -ENOMEM;
	}

	/* dynamically assign a new minor number */
	spin_lock(&omaprpc_services_lock);
	ret = idr_get_new(&omaprpc_services, rpcserv, &minor);
	if (ret == -EAGAIN) {
		spin_unlock(&omaprpc_services_lock);
		OMAPRPC_ERR(&rpdev->dev,
			    "OMAPRPC: Race to the new idr memory! (ret=%d)\n",
			    ret);
		goto again;
	} else if (ret != 0) {	/* probably -ENOSPC */
		spin_unlock(&omaprpc_services_lock);
		OMAPRPC_ERR(&rpdev->dev,
			    "OMAPRPC: failed to idr_get_new: %d\n", ret);
		return ret;
	}

	/*
	 * look for an already created rpc service
	 * and use that if the minor number matches
	 */
	list_for_each_entry(tmp, &omaprpc_services_list, list) {
		if (tmp->minor == minor) {
			rpcserv = tmp;
			idr_replace(&omaprpc_services, rpcserv, minor);
			break;
		}
	}
	spin_unlock(&omaprpc_services_lock);

	/* if we replaced a device, skip the creation */
	if (rpcserv)
		goto serv_up;

	/* Create a new character device and add it to the kernel /dev fs */
	rpcserv = kzalloc(sizeof(*rpcserv), GFP_KERNEL);
	if (!rpcserv) {
		OMAPRPC_ERR(&rpdev->dev, "OMAPRPC: kzalloc failed\n");
		ret = -ENOMEM;
		goto rem_idr;
	}

	/*
	 * Replace the pointer for the minor number, it shouldn't have existed
	 * (or was associated with NULL previously) so this is really an
	 * assignment
	 */
	spin_lock(&omaprpc_services_lock);
	idr_replace(&omaprpc_services, rpcserv, minor);
	spin_unlock(&omaprpc_services_lock);

	/* Initialize the instance list in the service */
	INIT_LIST_HEAD(&rpcserv->instance_list);

	/* Initialize the Mutex */
	mutex_init(&rpcserv->lock);

	/* Initialize the Completion lock */
	init_completion(&rpcserv->comp);

	/* Add this service to the list of services */
	list_add(&rpcserv->list, &omaprpc_services_list);

	/* get the assigned major number from the dev_t */
	major = MAJOR(omaprpc_dev);

	/* Create the character device */
	cdev_init(&rpcserv->cdev, &omaprpc_fops);
	rpcserv->cdev.owner = THIS_MODULE;

	/* Add the character device to the kernel */
	ret = cdev_add(&rpcserv->cdev, MKDEV(major, minor), 1);
	if (ret) {
		OMAPRPC_ERR(&rpdev->dev, "OMAPRPC: cdev_add failed: %d\n", ret);
		goto free_rpc;
	}

	ret = omaprpc_device_create(rpdev);
	if (ret) {
		OMAPRPC_ERR(&rpdev->dev,
			    "OMAPRPC: failed to query channel info: %d\n", ret);
		goto clean_cdev;
	}

serv_up:
	rpcserv->rpdev = rpdev;
	rpcserv->minor = minor;
	rpcserv->state = OMAPRPC_SERVICE_STATE_UP;

	/*
	 * Associate the service with the sysfs
	 * entry, this will be allow container_of to get the service pointer
	 */
	dev_set_drvdata(&rpdev->dev, rpcserv);

	/* Signal that the driver setup is complete */
	complete_all(&rpcserv->comp);

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, &rpdev->dev,
		      "OMAPRPC: new RPC connection srv channel: %u -> %u!\n",
		      rpdev->src, rpdev->dst);
	return 0;

clean_cdev:
	/* Failed to create sysfs entry, delete character device */
	cdev_del(&rpcserv->cdev);
free_rpc:
	/* Delete the allocated memory for the service */
	kfree(rpcserv);
rem_idr:
	/* Remove the minor number from our integer ID pool */
	spin_lock(&omaprpc_services_lock);
	idr_remove(&omaprpc_services, minor);
	spin_unlock(&omaprpc_services_lock);
	/* Return the set error */
	return ret;
}

static void __devexit omaprpc_remove(struct rpmsg_channel *rpdev)
{
	struct omaprpc_service_t *rpcserv = dev_get_drvdata(&rpdev->dev);
	int major = MAJOR(omaprpc_dev);
	struct omaprpc_instance_t *rpc = NULL;

	if (rpcserv == NULL) {
		OMAPRPC_ERR(&rpdev->dev, "Service was NULL\n");
		return;
	}

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
		      "OMAPRPC: removing rpmsg omaprpc driver %u.%u\n",
		      major, rpcserv->minor);

	spin_lock(&omaprpc_services_lock);
	idr_remove(&omaprpc_services, rpcserv->minor);
	spin_unlock(&omaprpc_services_lock);

	mutex_lock(&rpcserv->lock);

	/* If there are no instances in the list, just teardown */
	if (list_empty(&rpcserv->instance_list)) {
		device_destroy(omaprpc_class, MKDEV(major, rpcserv->minor));
		cdev_del(&rpcserv->cdev);
		list_del(&rpcserv->list);
		mutex_unlock(&rpcserv->lock);
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, &rpdev->dev,
			      "OMAPRPC: no instances, removed driver!\n");
		kfree(rpcserv);
		return;
	}

	/*
	 * If there are rpc instances that means that this is a recovery
	 * operation. Don't clean the rpcserv. Each instance may be in a
	 * weird state.
	 */
	init_completion(&rpcserv->comp);
	rpcserv->state = OMAPRPC_SERVICE_STATE_DOWN;
	list_for_each_entry(rpc, &rpcserv->instance_list, list) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpcserv->dev,
			      "Instance %p in state %d\n", rpc, rpc->state);
		/* set rpc instance to fault state */
		rpc->state = OMAPRPC_STATE_FAULT;
		/* complete any on-going transactions */
		if ((rpc->state == OMAPRPC_STATE_CONNECTED ||
		     rpc->state == OMAPRPC_STATE_DISCONNECTED) &&
		    rpc->transisioning) {
			/*
			 * we were in the middle of
			 * connecting or disconnecting
			 */
			complete_all(&rpc->reply_arrived);
		}
		/* wake up anyone waiting on a read */
		wake_up_interruptible(&rpc->readq);
	}
	mutex_unlock(&rpcserv->lock);
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, &rpdev->dev,
		      "OMAPRPC: removed rpmsg omaprpc driver.\n");
}

static void omaprpc_driver_cb(struct rpmsg_channel *rpdev,
			      void *data, int len, void *priv, u32 src)
{
	struct omaprpc_service_t *rpcserv = dev_get_drvdata(&rpdev->dev);

	struct omaprpc_msg_header_t *hdr = data;
	struct omaprpc_channel_info_t *info;
	char *buf = (char *)data;
	u32 expected = 0;
	int major = 0;

	expected = sizeof(struct omaprpc_msg_header_t);
	switch (hdr->msg_type) {
	case OMAPRPC_MSG_CHAN_INFO:
		expected += sizeof(struct omaprpc_channel_info_t);
		break;
	}

	if (len < expected) {
		OMAPRPC_ERR(&rpdev->dev,
			"OMAPRPC driver: truncated message detected! "
			"Was %u bytes long, expected %u\n", len, expected);
		return;
	}

	switch (hdr->msg_type) {
	case OMAPRPC_MSG_CHAN_INFO:
		major = MAJOR(omaprpc_dev);
		info = OMAPRPC_PAYLOAD(buf, omaprpc_channel_info_t);
		info->name[sizeof(info->name) - 1] = '\0';

		if (rpcserv->dev) {
			OMAPRPC_ERR(&rpdev->dev,
				    "OMAPRPC: device already created!\n");
			break;
		}

		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, &rpdev->dev,
			      "OMAPRPC: creating device: %s\n",
			      info->name);
		/* Create the /dev sysfs entry */
		rpcserv->dev = device_create(omaprpc_class, &rpdev->dev,
					     MKDEV(major,
						   rpcserv->minor),
					     NULL, info->name);
		if (IS_ERR(rpcserv->dev)) {
			int ret = PTR_ERR(rpcserv->dev);
			dev_err(&rpdev->dev,
				"OMAPRPC: device_create failed: %d\n",
				ret);
			/* TODO:
			 * is this cleanup enough?
			 * At this point probe has succeded
			 */
			/*
			 * Failed to create sysfs entry, delete
			 * character device
			 */
			cdev_del(&rpcserv->cdev);
			dev_set_drvdata(&rpdev->dev, NULL);
			/*
			 * Remove the minor number from our integer
			 * ID pool
			 */
			spin_lock(&omaprpc_services_lock);
			idr_remove(&omaprpc_services, rpcserv->minor);
			spin_unlock(&omaprpc_services_lock);
			/* Delete the allocated memory for the service
			 */
			kfree(rpcserv);
		}
		break;
	default:
		OMAPRPC_ERR(&rpdev->dev, "OMAPRPC: Unrecognized code %u\n",
			hdr->msg_type);
		break;
	}
}

static struct rpmsg_device_id omaprpc_id_table[] = {
	{.name = "omaprpc"},
	{},
};

MODULE_DEVICE_TABLE(rpmsg, omaprpc_id_table);

static struct rpmsg_driver omaprpc_driver = {
	.drv.name = KBUILD_MODNAME,
	.drv.owner = THIS_MODULE,
	.id_table = omaprpc_id_table,
	.probe = omaprpc_probe,
	.remove = __devexit_p(omaprpc_remove),
	.callback = omaprpc_driver_cb,
};

static int __init omaprpc_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&omaprpc_dev, 0, OMAPRPC_CORE_REMOTE_MAX,
				  KBUILD_MODNAME);
	if (ret) {
		pr_err("OMAPRPC: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	omaprpc_class = class_create(THIS_MODULE, KBUILD_MODNAME);
	if (IS_ERR(omaprpc_class)) {
		ret = PTR_ERR(omaprpc_class);
		pr_err("OMAPRPC: class_create failed: %d\n", ret);
		goto unreg_region;
	}

	ret = register_rpmsg_driver(&omaprpc_driver);
	pr_err("OMAPRPC: Registration of OMAPRPC rpmsg service returned %d! ",
	       ret);
	pr_err("OMAPRPC: debug=%d\n", omaprpc_debug);
	return ret;
unreg_region:
	unregister_chrdev_region(omaprpc_dev, OMAPRPC_CORE_REMOTE_MAX);
	return ret;
}

module_init(omaprpc_init);

static void __exit omaprpc_fini(void)
{
	struct omaprpc_service_t *rpcserv, *tmp;
	int major = MAJOR(omaprpc_dev);

	unregister_rpmsg_driver(&omaprpc_driver);
	list_for_each_entry_safe(rpcserv, tmp, &omaprpc_services_list, list) {
		device_destroy(omaprpc_class, MKDEV(major, rpcserv->minor));
		cdev_del(&rpcserv->cdev);
		list_del(&rpcserv->list);
		kfree(rpcserv);
	}
	class_destroy(omaprpc_class);
	unregister_chrdev_region(omaprpc_dev, OMAPRPC_CORE_REMOTE_MAX);
}

module_exit(omaprpc_fini);

MODULE_AUTHOR("Erik Rainey <erik.rainey@ti.com>");
MODULE_DESCRIPTION("OMAP Remote Procedure Call Driver");
MODULE_ALIAS("rpmsg:omaprpc");
MODULE_LICENSE("GPL v2");
