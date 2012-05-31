/*
 * Remote processor resource manager
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * Fernando Guzman Lugo <fernando.lugo@ti.com>
 * Miguel Vadillo <vadillo@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/idr.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg_resmgr.h>

#define MAX_RES_SIZE 128
#define MAX_MSG (sizeof(struct rprm_msg) + sizeof(struct rprm_request) +\
		MAX_RES_SIZE)
#define MAX_RES_BUF 512
#define MAX_DENTRY_SIZE 128

/**
 * struct rprm_elem - an instance of this struct represents a resource
 * @next: pointer to next element
 * @res: pointer to the rprm_res structure which this element belongs to
 * @handle: stores handle returned by low level modules
 * @id: resource id
 * @base: base address of the resource registers (will be mapped to the rproc
 *	  address space in the future)
 * @constraints: stores the constraints requested for the resource
 */
struct rprm_elem {
	struct list_head next;
	struct rprm_res *res;
	void *handle;
	u32 id;
	u32 base;
	struct rprm_constraints_data constraints;
};

/**
 * struct rprm - remoteproc resource manager instance
 * @res_list: list to the resources that belongs to this channel
 * @id_list: idrs of the resources
 * @lock: protection
 * @dentry: debug entry
 * @rpdev: pointer to the channel of the connection.
 */
struct rprm {
	struct list_head res_list;
	struct idr id_list;
	struct mutex lock;
	struct dentry *dentry;
	struct rpmsg_channel *rpdev;
};

/* default constraint values for removing constraints */
static struct rprm_constraints_data def_data = {
	.frequency	= 0,
	.bandwidth	= -1,
	.latency	= -1,
};

/* List of availabe resources */
static LIST_HEAD(res_table);
static DEFINE_SPINLOCK(table_lock);

static struct rprm_res *__find_res_by_name(const char *name)
{
	struct rprm_res *res;

	list_for_each_entry(res, &res_table, next)
		if (!strcmp(res->name, name))
			return res;

	return NULL;
}

static int _set_constraints(struct device *dev, struct rprm_elem *e,
				struct rprm_constraints_data *c)
{
	int ret = 0;
	u32 mask = 0;

	if (c->mask & RPRM_SCALE) {
		if (!e->res->ops->scale) {
			ret = -ENOSYS;
			goto err;
		}

		ret = e->res->ops->scale(dev, e->handle, c->frequency);
		if (ret)
			goto err;
		mask |= RPRM_SCALE;
		e->constraints.frequency = c->frequency;
	}

	if (c->mask & RPRM_LATENCY) {
		if (!e->res->ops->latency) {
			ret = -ENOSYS;
			goto err;
		}

		ret = e->res->ops->latency(dev, e->handle, c->latency);
		if (ret)
			goto err;
		mask |= RPRM_LATENCY;
		e->constraints.latency = c->latency;
	}

	if (c->mask & RPRM_BANDWIDTH) {
		if (!e->res->ops->bandwidth) {
			ret = -ENOSYS;
			goto err;
		}

		ret = e->res->ops->bandwidth(dev, e->handle, c->bandwidth);
		if (ret)
			goto err;
		mask |= RPRM_BANDWIDTH;
		e->constraints.bandwidth = c->bandwidth;
	}
err:
	if (ret)
		dev_err(dev, "error updating contraints for %s:\n"
			"Mask:0x%x\n"
			"Frequency:%ld\n"
			"Latency:%ld\n"
			"Bandwidth:%ld\n",
			e->res->name, c->mask, c->frequency,
			c->latency, c->bandwidth);
	/*
	 * even in case of a error update the mask with the constraints that
	 * were set, so that those constraints can be removed
	 */
	c->mask = mask;
	return ret;
}

/* update constaints for a resource */
static int rprm_update_constraints(struct rprm *rprm, int res_id,
			   struct rprm_constraints_data *data, bool set)
{
	int ret = 0;
	struct rprm_elem *e;
	struct device *dev = &rprm->rpdev->dev;

	mutex_lock(&rprm->lock);
	e = idr_find(&rprm->id_list, res_id);
	if (!e) {
		ret = -ENOENT;
		goto out;
	}

	dev_dbg(dev, "%s contraint for %s:\n"
		"Mask:0x%x\n"
		"Frequency:%ld\n"
		"Latency:%ld\n"
		"Bandwidth:%ld\n",
		set ? "setting" : "clearing", e->res->name, data->mask,
		data->frequency, data->latency, data->bandwidth);

	if (set) {
		ret = _set_constraints(dev, e, data);
		if (!ret) {
			e->constraints.mask |= data->mask;
			goto out;
		}
		/* if error continue to remove the constraints that were set */
	}

	/* use def data structure values in order to remove constraints */
	def_data.mask = data->mask;
	if (def_data.mask) {
		int tmp;

		tmp = _set_constraints(dev, e, &def_data);
		if (!tmp)
			e->constraints.mask &= ~data->mask;
		/* do not overwrite ret if there was a previous error */
		ret = ret ? : tmp;
	}
out:
	mutex_unlock(&rprm->lock);
	return ret;
}

static int _resource_release(struct rprm *rprm, struct rprm_elem *e)
{
	int ret;
	struct device *dev = &rprm->rpdev->dev;

	dev_dbg(dev, "releasing %s resource\n", e->res->name);

	/* if there are constraints then remove them */
	if (e->constraints.mask) {
		def_data.mask = e->constraints.mask;
		 _set_constraints(dev, e, &def_data);
		e->constraints.mask &= ~def_data.mask;
	}

	if (e->res->ops->release) {
		ret = e->res->ops->release(e->handle);
		if (ret) {
			dev_err(dev, "failed to release %s ret %d\n",
							e->res->name, ret);
			return ret;
		}
	}

	list_del(&e->next);
	module_put(e->res->owner);
	kfree(e);

	return 0;
}

static int rprm_resource_release(struct rprm *rprm, int res_id)
{
	struct device *dev = &rprm->rpdev->dev;
	struct rprm_elem *e;
	int ret;

	mutex_lock(&rprm->lock);
	e = idr_find(&rprm->id_list, res_id);
	if (!e) {
		dev_err(dev, "invalid resource id\n");
		ret = -ENOENT;
		goto out;
	}

	ret = _resource_release(rprm, e);
	if (ret)
		goto out;

	idr_remove(&rprm->id_list, res_id);
out:
	mutex_unlock(&rprm->lock);
	return ret;
}

static int rprm_resource_request(struct rprm *rprm, const char *name,
				int *res_id, void *data, size_t len)
{
	struct device *dev = &rprm->rpdev->dev;
	struct rprm_elem *e;
	struct rprm_res *res;
	int ret;

	dev_dbg(dev, "requesting %s with data len %d\n", name, len);

	/* resource must be registered */
	res = __find_res_by_name(name);
	if (!res) {
		dev_err(dev, "resource no valid %s\n", name);
		return -ENOENT;
	}

	/* prevent underlying implementation from being removed */
	if (!try_module_get(res->owner)) {
		dev_err(dev, "can't get owner\n");
		return -EINVAL;
	}

	if (!res->ops->request) {
		ret = -ENOSYS;
		goto err_module;
	}

	e = kzalloc(sizeof *e, GFP_KERNEL);
	if (!e) {
		ret = -ENOMEM;
		goto err_module;
	}

	mutex_lock(&rprm->lock);
	ret = res->ops->request(&e->handle, data, len);
	if (ret) {
		dev_err(dev, "request for %s failed: %d\n", name, ret);
		goto err_free;
	}

	/*
	 * Create a resource id to avoid sending kernel address to the
	 * remote processor.
	 */
	if (!idr_pre_get(&rprm->id_list, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto err_release;
	}

	ret = idr_get_new(&rprm->id_list, e, res_id);
	if (ret)
		goto err_release;

	e->id = *res_id;
	e->res = res;
	list_add(&e->next, &rprm->res_list);
	mutex_unlock(&rprm->lock);

	return 0;
err_release:
	if (res->ops->release)
		res->ops->release(e->handle);
err_free:
	kfree(e);
	mutex_unlock(&rprm->lock);
err_module:
	module_put(res->owner);
	return ret;
}

static void rprm_cb(struct rpmsg_channel *rpdev, void *data, int len,
			void *priv, u32 src)
{
	struct device *dev = &rpdev->dev;
	struct rprm *rprm = dev_get_drvdata(dev);
	struct rprm_msg *msg = data;
	struct rprm_request *req;
	struct rprm_release *rel;
	struct rprm_constraint *c;
	char ack_msg[MAX_MSG];
	struct rprm_ack *ack = (void *)ack_msg;
	struct rprm_request_ack *rack = (void *)ack->data;
	int res_id;
	int ret;
	bool set;

	dev_dbg(dev, "resmgr msg from %u and len %d\n" , src, len);

	len -= sizeof *msg;
	if (len < 0) {
		dev_err(dev, "Bad message: no message header\n");
		return;
	}

	/* we only accept action request from established channels */
	if (rpdev->dst != src) {
		dev_err(dev, "remote endpoint %d not connected to this resmgr\n"
			"channel, expected %d endpoint\n", src, rpdev->dst);
		ret = -ENOTCONN;
		goto out;
	}

	dev_dbg(dev, "resmgr action %d\n" , msg->action);

	switch (msg->action) {
	case RPRM_REQUEST:
		len -= sizeof *req;
		if (len < 0) {
			dev_err(dev, "Bad message: no request header\n");
			ret = -EINVAL;
			break;
		}

		req = (void *)msg->data;
		/* make sure NULL terminated resource name */
		req->res_name[sizeof req->res_name - 1] = '\0';
		ret = rprm_resource_request(rprm, req->res_name, &res_id,
								req->data, len);
		if (ret) {
			dev_err(dev, "resource allocation failed %d!\n", ret);
			break;
		}

		rack->res_id = res_id;
		memcpy(rack->data, req->data, len);
		len += sizeof *rack;
		break;
	case RPRM_RELEASE:
		len -= sizeof *rel;
		if (len < 0) {
			dev_err(dev, "Bad message: no release header\n");
			return;
		}

		rel = (void *)msg->data;
		ret = rprm_resource_release(rprm, rel->res_id);
		if (ret)
			dev_err(dev, "resource release failed %d!\n", ret);
		/* no ack for release resource */
		return;
	case RPRM_SET_CONSTRAINTS:
	case RPRM_CLEAR_CONSTRAINTS:
		len -= sizeof *c;
		if (len < 0) {
			dev_err(dev, "Bad message\n");
			return;
		}

		set = msg->action == RPRM_SET_CONSTRAINTS;
		c = (void *)msg->data;
		ret = rprm_update_constraints(rprm, c->res_id, &c->cdata, set);
		if (ret)
			dev_err(dev, "%s constraints failed! ret %d\n",
				set ? "set" : "clear", ret);

		/* no ack when removing constraints */
		if (!set)
			return;

		len = sizeof *c;
		memcpy(ack->data, c, len);
		break;
	default:
		dev_err(dev, "Unknow action %d\n", msg->action);
		ret = -EINVAL;
	}
out:
	ack->action = msg->action;
	ack->ret = ret;
	len = ret ? 0 : len;
	ret = rpmsg_sendto(rpdev, ack, sizeof *ack + len, src);
	if (ret)
		dev_err(dev, "rprm ack failed: %d\n", ret);
}

/* print constraint information for a resource */
static int _snprint_constraints_info(struct rprm_elem *e, char *buf, size_t len)
{
	return snprintf(buf, len,
		"Mask:0x%x\n"
		"Frequency:%ld\n"
		"Latency:%ld\n"
		"Bandwidth:%ld\n",
		e->constraints.mask, e->constraints.frequency,
		e->constraints.latency, e->constraints.bandwidth);
}

static ssize_t rprm_dbg_read(struct file *filp, char __user *userbuf,
			 size_t count, loff_t *ppos)
{
	struct rprm *rprm = filp->private_data;
	struct rprm_elem *e;
	char buf[MAX_RES_BUF];
	int total = 0, c, tmp;
	loff_t p = 0, pt;
	size_t len = MAX_RES_BUF;

	mutex_lock(&rprm->lock);
	c = snprintf(buf, len, "## resource list for remote endpoint %d ##\n",
							rprm->rpdev->src);
	list_for_each_entry(e, &rprm->res_list, next) {
		c += snprintf(buf + c, len - c , "\n-resource name:%s\n",
							e->res->name);
		if (e->res->ops->get_info)
			c += e->res->ops->get_info(e->handle,
						buf + c, len - c);

		if (e->constraints.mask)
			c += _snprint_constraints_info(e, buf + c, len - c);

		p += c;
		/* if resource already read then continue */
		if (*ppos >= p) {
			c = 0;
			continue;
		}

		pt = c - p + *ppos;
		tmp = simple_read_from_buffer(userbuf + total, count, &pt,
						 buf, c);
		total += tmp;
		*ppos += tmp;
		/* if userbuf is full then break */
		if (tmp - c)
			break;
		c = 0;
	}
	mutex_unlock(&rprm->lock);

	return total;
}

static int rprm_dbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations rprm_dbg_ops = {
	.read = rprm_dbg_read,
	.open = rprm_dbg_open,
	.llseek	= generic_file_llseek,
};

/**
 * rprm_resource_register - register a new resource
 * @res: pointer to the resource structure
 *
 * This function registers a new resource @res so that, a remote processor
 * can request it and then release it when it is not needed anymore.
 *
 * On success 0 is returned. Otherwise, will return proper error.
 */
int rprm_resource_register(struct rprm_res *res)
{
	int ret = 0;

	if (!res || !res->name || !res->ops)
		return -EINVAL;

	spin_lock(&table_lock);
	/* resources cannot have the same name */
	if (__find_res_by_name(res->name)) {
		pr_err("resource %s already exists!\n", res->name);
		ret = -EEXIST;
		goto unlock;
	}

	list_add_tail(&res->next, &res_table);
	pr_debug("registering resource %s\n", res->name);
unlock:
	spin_unlock(&table_lock);

	return ret;
}
EXPORT_SYMBOL(rprm_resource_register);

/**
 * rprm_resource_unregister - unregister a resource
 * @res: pointer to the resource structure
 *
 * This function unregisters a resource @res previously registered. After this
 * the remote processor cannot request @res anymore.
 *
 * On success 0 is returned. Otherwise, -EINVAL is returned.
 */
int rprm_resource_unregister(struct rprm_res *res)
{
	if (!res)
		return -EINVAL;

	spin_lock(&table_lock);
	list_del(&res->next);
	spin_unlock(&table_lock);
	pr_debug("unregistering resource %s\n", res->name);

	return 0;
}
EXPORT_SYMBOL(rprm_resource_unregister);

/* probe function is called every time a new connection(device) is created */
static int rprm_probe(struct rpmsg_channel *rpdev)
{
	struct rprm *rprm;
	struct rprm_ack ack;
	struct rproc *rproc = vdev_to_rproc(rpdev->vrp->vdev);
	char name[MAX_DENTRY_SIZE];
	int ret = 0;

	rprm = kmalloc(sizeof *rprm, GFP_KERNEL);
	if (!rprm) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_init(&rprm->lock);
	idr_init(&rprm->id_list);
	/*
	 * Beside the idr we create a linked list, that way we can cleanup the
	 * resources allocated in reverse order as they were requested which
	 * is safer.
	 */
	INIT_LIST_HEAD(&rprm->res_list);
	rprm->rpdev = rpdev;
	dev_set_drvdata(&rpdev->dev, rprm);

	snprintf(name, MAX_DENTRY_SIZE, "resmgr-%s", dev_name(&rpdev->dev));
	name[MAX_DENTRY_SIZE - 1] = '\0';
	/*
	 * Create a debug entry in the same rproc directory so that we can know
	 * which remote proc is using this resmgr connection
	 */
	rprm->dentry = debugfs_create_file(name, 0400, rproc->dbg_dir, rprm,
							&rprm_dbg_ops);
out:
	/* send a message back to ack the connection */
	ack.action = RPRM_CONNECT;
	ack.ret = ret;
	if (rpmsg_send(rpdev, &ack, sizeof ack))
		dev_err(&rpdev->dev, "error sending respond!\n");

	return ret;
}

/* remove function is called when the connection is terminated */
static void __devexit rprm_remove(struct rpmsg_channel *rpdev)
{
	struct rprm *rprm = dev_get_drvdata(&rpdev->dev);
	struct rprm_elem *e, *tmp;

	/* clean up remaining resources */
	mutex_lock(&rprm->lock);
	list_for_each_entry_safe(e, tmp, &rprm->res_list, next)
		_resource_release(rprm, e);

	idr_remove_all(&rprm->id_list);
	idr_destroy(&rprm->id_list);
	mutex_unlock(&rprm->lock);
	if (rprm->dentry)
		debugfs_remove(rprm->dentry);

	kfree(rprm);
}

static struct rpmsg_device_id rprm_id_table[] = {
	{
		.name	= "rpmsg-resmgr",
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, rprm_id_table);

static struct rpmsg_driver rprm_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rprm_id_table,
	.probe		= rprm_probe,
	.callback	= rprm_cb,
	.remove		= __devexit_p(rprm_remove),
};

static int __init rprm_init(void)
{
	return register_rpmsg_driver(&rprm_driver);
}
module_init(rprm_init);

static void __exit rprm_fini(void)
{
	unregister_rpmsg_driver(&rprm_driver);
}
module_exit(rprm_fini);

MODULE_DESCRIPTION("Remote Processor Resource Manager");
MODULE_LICENSE("GPL v2");
