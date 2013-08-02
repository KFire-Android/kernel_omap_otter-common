/*
 * Mailbox framework
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 * Author: Loic Pallardy <loic.pallardy@st.com> for ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/module.h>

#include "mailbox_internal.h"

/* global variables for the mailbox devices */
static DEFINE_MUTEX(mailbox_devices_lock);
static LIST_HEAD(mailbox_devices);

/*
 * default size for the fifos, configured through kernel menuconfig
 * TODO: Need to move this into the individual driver implementations, and
 *	 pass the fifo size as a mailbox configuration value.
 */
static unsigned int mbox_kfifo_size = CONFIG_MBOX_KFIFO_SIZE;
module_param(mbox_kfifo_size, uint, S_IRUGO);
MODULE_PARM_DESC(mbox_kfifo_size, "Size of mailbox kfifo (bytes)");

/* mailbox h/w transport communication handler helper functions */
static inline void mbox_read(struct mailbox *mbox, struct mailbox_msg *msg)
{
	mbox->ops->read(mbox, msg);
}
static inline int mbox_write(struct mailbox *mbox, struct mailbox_msg *msg)
{
	return mbox->ops->write(mbox, msg);
}
static inline int mbox_empty(struct mailbox *mbox)
{
	return mbox->ops->empty(mbox);
}
static inline int mbox_needs_flush(struct mailbox *mbox)
{
	return (mbox->ops->needs_flush ? mbox->ops->needs_flush(mbox) : 0);
}
static inline void mbox_fifo_readback(struct mailbox *mbox,
					struct mailbox_msg *msg)
{
	if (mbox->ops->readback)
		mbox->ops->readback(mbox, msg);
}

/* mailbox h/w irq handler helper functions */
static inline void ack_mbox_irq(struct mailbox *mbox, mailbox_irq_t irq)
{
	if (mbox->ops->ack_irq)
		mbox->ops->ack_irq(mbox, irq);
}
static inline int is_mbox_irq(struct mailbox *mbox, mailbox_irq_t irq)
{
	return mbox->ops->is_irq(mbox, irq);
}

/*
 * local helper to check if the h/w transport is busy or free.
 * Returns 0 if free, and non-zero otherwise
 */
static int __mbox_poll_for_space(struct mailbox *mbox)
{
	return mbox->ops->poll_for_space(mbox);
}

/**
 * mailbox_msg_send() - send a mailbox message
 * @mbox: handle to the acquired mailbox on which to send the message
 * @msg: the mailbox message to be sent
 *
 * This API is called by a client user to send a mailbox message on an
 * acquired mailbox. The API currently cannot be called in an atomic
 * context. The API transmits the message immediately on the h/w
 * communication transport if it is available, otherwise buffers the
 * message for transmission as soon as the h/w transport is ready.
 *
 * The only failure from this function is when neither the h/w transport
 * is available nor the s/w buffer fifo is empty.
 *
 * Returns 0 on success, or an error otherwise
 */
int mailbox_msg_send(struct mailbox *mbox, struct mailbox_msg *msg)
{
	struct mailbox_queue *mq = mbox->txq;
	int ret = 0, len;

	mutex_lock(&mq->mlock);

	if (kfifo_avail(&mq->fifo) < (sizeof(*msg) + msg->size)) {
		ret = -ENOMEM;
		goto out;
	}

	if (kfifo_is_empty(&mq->fifo) && !__mbox_poll_for_space(mbox)) {
		ret = mbox_write(mbox, msg);
		goto out;
	}

	len = kfifo_in(&mq->fifo, (unsigned char *)msg, sizeof(*msg));
	WARN_ON(len != sizeof(*msg));

	if (msg->size && msg->pdata) {
		len = kfifo_in(&mq->fifo, (unsigned char *)msg->pdata,
								msg->size);
		WARN_ON(len != msg->size);
	}

	tasklet_schedule(&mbox->txq->tasklet);

out:
	mutex_unlock(&mq->mlock);
	return ret;
}
EXPORT_SYMBOL(mailbox_msg_send);

/*
 * Empty the Tx FIFO by reading back the messages. This function
 * is mainly for usecases where the receiver is capable of
 * receiving the interrupt, but does not have access to the
 * mailbox registers.
 *
 * Returns the no. of messages read back
 */
int mailbox_empty_tx(struct mailbox *mbox)
{
	int ret = 0;
	struct mailbox_msg msg;

	while (mbox_needs_flush(mbox)) {
		mbox_fifo_readback(mbox, &msg);
		ret++;
	}

	/* no more messages in the fifo, clear IRQ source */
	if (ret)
		ack_mbox_irq(mbox, IRQ_RX);

	return ret;
}
EXPORT_SYMBOL(mailbox_empty_tx);

#define TRANSFER_TIMEOUT 30000 /* Becomes ~3s timeout */

static struct mailbox_msg no_irq_msg_res;

/**
 * mailbox_msg_send_receive_no_irq() - send and receive a mailbox message
 * @mbox: handle to the acquired mailbox on which to send and receive the
 *	  message
 * @msg: the mailbox message to be sent
 *
 * This API is called by a client user to send a mailbox message and also
 * receive a response in a normal context. The API cannot be called in an
 * atomic context. It is blocking, and sleeps upon transmission until a
 * response is received or a default timeout expires (currently set at 3
 * seconds).
 *
 * The API expects the h/w transport to be available for transmission. It
 * is assumed that no failures occur during transmission.
 *
 * Returns the received mailbox message on success.
 */
struct mailbox_msg *mailbox_msg_send_receive_no_irq(struct mailbox *mbox,
		struct mailbox_msg *msg)
{
	int ret = 0;
	int count = 0;

	BUG_ON(!irqs_disabled());

	if (likely(mbox->ops->write && mbox->ops->read)) {
		if (__mbox_poll_for_space(mbox)) {
			ret = -EBUSY;
			goto out;
		}
		mbox->ops->write(mbox, msg);
		while (!is_mbox_irq(mbox, IRQ_RX)) {
			udelay(100);
			cpu_relax();
			count++;
			if (count > TRANSFER_TIMEOUT) {
				pr_err("%s: Error: transfer timed out\n",
						__func__);
				ret = -EINVAL;
				goto out;
			}
		}
		mbox->ops->read(mbox, &no_irq_msg_res);
		ack_mbox_irq(mbox, IRQ_RX);
	} else {
		ret = -EINVAL;
	}

out:
	BUG_ON(ret < 0);

	return &no_irq_msg_res;
}
EXPORT_SYMBOL(mailbox_msg_send_receive_no_irq);

/**
 * mailbox_msg_send_no_irq() - send a mailbox message without buffering
 * @mbox: handle to the acquired mailbox on which to the message
 * @msg: the mailbox message to be sent
 *
 * This API is called by a client user to send a mailbox message without
 * performing any buffering. The API cannot be called in an atomic context.
 *
 * The API expects the h/w transport to be available for transmission. It
 * is assumed that no failures occur during transmission.
 *
 * Returns 0 on successful transmission, or an error otherwise
 */
int mailbox_msg_send_no_irq(struct mailbox *mbox,
		struct mailbox_msg *msg)
{
	int ret = 0;

	BUG_ON(!irqs_disabled());

	if (likely(mbox->ops->write)) {
		if (__mbox_poll_for_space(mbox)) {
			ret = -EBUSY;
			goto out;
		}
		mbox->ops->write(mbox, msg);
	} else {
		ret = -EINVAL;
	}

out:
	WARN_ON(ret < 0);

	return ret;
}
EXPORT_SYMBOL(mailbox_msg_send_no_irq);

/**
 * mailbox_save_ctx: save the context of a mailbox
 * @mbox: handle to the acquired mailbox
 *
 * This allows a client (controlling a remote) to request a mailbox to
 * save its context when it is powering down the remote.
 *
 * NOTE: This will be deprecated, new clients should not use this.
 *       The same feature can be enabled through runtime_pm enablement
 *	 of mailbox.
 */
void mailbox_save_ctx(struct mailbox *mbox)
{
	if (!mbox->ops->save_ctx) {
		dev_err(mbox->dev, "%s:\tno save\n", __func__);
		return;
	}

	mbox->ops->save_ctx(mbox);
}
EXPORT_SYMBOL(mailbox_save_ctx);

/**
 * mailbox_restore_ctx: restore the context of a mailbox
 * @mbox: handle to the acquired mailbox
 *
 * This allows a client (controlling a remote) to request a mailbox to
 * restore its context after restoring the remote, so that it can
 * communicate with the remote as it would normally.
 *
 * NOTE: This will be deprecated, new clients should not use this.
 *       The same feature can be enabled through runtime_pm enablement
 *	 of mailbox.
 */
void mailbox_restore_ctx(struct mailbox *mbox)
{
	if (!mbox->ops->restore_ctx) {
		dev_err(mbox->dev, "%s:\tno restore\n", __func__);
		return;
	}

	mbox->ops->restore_ctx(mbox);
}
EXPORT_SYMBOL(mailbox_restore_ctx);

/**
 * mailbox_enable_irq: enable a specific mailbox Rx or Tx interrupt source
 * @mbox: handle to the acquired mailbox
 * @irq: interrupt type associated with either the Rx or Tx
 *
 * This allows a client (having its own shared memory communication protocal
 * with the remote) to request a mailbox to enable a particular interrupt
 * signal source of the mailbox, as part of its communication state machine.
 *
 * NOTE: This will be deprecated, new clients should not use this. It is
 *	 being exported for TI DSP/Bridge driver.
 */
void mailbox_enable_irq(struct mailbox *mbox, mailbox_irq_t irq)
{
	mbox->ops->enable_irq(mbox, irq);
}
EXPORT_SYMBOL(mailbox_enable_irq);

/**
 * mailbox_disable_irq: disable a specific mailbox Rx or Tx interrupt source
 * @mbox: handle to the acquired mailbox
 * @irq: interrupt type associated with either the Rx or Tx
 *
 * This allows a client (having its own shared memory communication protocal
 * with the remote) to request a mailbox to disable a particular interrupt
 * signal source of the mailbox, as part of its communication state machine.
 *
 * NOTE: This will be deprecated, new clients should not use this. It is
 *	 being exported for TI DSP/Bridge driver.
 */
void mailbox_disable_irq(struct mailbox *mbox, mailbox_irq_t irq)
{
	mbox->ops->disable_irq(mbox, irq);
}
EXPORT_SYMBOL(mailbox_disable_irq);

/*
 * This is the tasklet function in which all the buffered messages are
 * sent until the h/w transport is busy again. The tasklet is scheduled
 * upon receiving an interrupt indicating the availability of the h/w
 * transport.
 */
static void mbox_tx_tasklet(unsigned long tx_data)
{
	struct mailbox *mbox = (struct mailbox *)tx_data;
	struct mailbox_queue *mq = mbox->txq;
	struct mailbox_msg msg;
	int ret;
	unsigned char tx_data_buf[CONFIG_MBOX_DATA_SIZE];

	while (kfifo_len(&mq->fifo)) {
		if (__mbox_poll_for_space(mbox)) {
			mailbox_enable_irq(mbox, IRQ_TX);
			break;
		}

		ret = kfifo_out(&mq->fifo, (unsigned char *)&msg, sizeof(msg));
		WARN_ON(ret != sizeof(msg));

		if (msg.size) {
			ret = kfifo_out(&mq->fifo, tx_data_buf,
							sizeof(msg.size));
			WARN_ON(ret != msg.size);
			msg.pdata = tx_data_buf;
		}

		ret = mbox_write(mbox, &msg);
		WARN_ON(ret);
	}
}

static unsigned char rx_work_data[CONFIG_MBOX_DATA_SIZE];

/*
 * This is the message receiver workqueue function, which is responsible
 * for delivering all the received messages stored in the receive kfifo
 * to the clients. Each message is delivered to all the registered mailbox
 * clients. It also re-enables the receive interrupt on the mailbox (disabled
 * when the s/w kfifo is full) after emptying atleast a message from the
 * fifo.
 */
static void mbox_rx_work(struct work_struct *work)
{
	struct mailbox_queue *mq =
		container_of(work, struct mailbox_queue, work);
	int len;
	struct mailbox *mbox = mq->mbox;
	struct mailbox_msg msg;

	while (kfifo_len(&mq->fifo) >= sizeof(msg)) {
		len = kfifo_out(&mq->fifo, (unsigned char *)&msg, sizeof(msg));
		WARN_ON(len != sizeof(msg));

		if (msg.size) {
			len = kfifo_out(&mq->fifo, rx_work_data, msg.size);
			WARN_ON(len != msg.size);
			msg.pdata = rx_work_data;
		}

		blocking_notifier_call_chain(&mbox->notifier, len,
								(void *)&msg);
		spin_lock_irq(&mq->lock);
		if (mq->full) {
			mq->full = false;
			mailbox_enable_irq(mbox, IRQ_RX);
		}
		spin_unlock_irq(&mq->lock);
	}
}

/*
 * Interrupt handler for Tx interrupt source for each of the mailboxes.
 * This schedules the tasklet to transmit the messages buffered in the
 * Tx fifo.
 */
static void __mbox_tx_interrupt(struct mailbox *mbox)
{
	mailbox_disable_irq(mbox, IRQ_TX);
	ack_mbox_irq(mbox, IRQ_TX);
	tasklet_schedule(&mbox->txq->tasklet);
}

/*
 * Interrupt handler for Rx interrupt source for each of the mailboxes.
 * This performs the read from the h/w mailbox until the transport is
 * free of any incoming messages, and buffers the read message. The
 * buffers are delivered to clients by scheduling a work-queue.
 */
static void __mbox_rx_interrupt(struct mailbox *mbox)
{
	struct mailbox_queue *mq = mbox->rxq;
	struct mailbox_msg msg;
	int len;

	while (!mbox_empty(mbox)) {
		if (unlikely(kfifo_avail(&mq->fifo) <
				(sizeof(msg) + CONFIG_MBOX_DATA_SIZE))) {
			mailbox_disable_irq(mbox, IRQ_RX);
			mq->full = true;
			goto nomem;
		}

		mbox_read(mbox, &msg);

		len = kfifo_in(&mq->fifo, (unsigned char *)&msg, sizeof(msg));
		WARN_ON(len != sizeof(msg));

		if (msg.pdata && msg.size) {
			len = kfifo_in(&mq->fifo, (unsigned char *)msg.pdata,
					msg.size);
			WARN_ON(len != msg.size);
		}
	}

	/* no more messages in the fifo. clear IRQ source. */
	ack_mbox_irq(mbox, IRQ_RX);
nomem:
	schedule_work(&mbox->rxq->work);
}

/*
 * The core mailbox interrupt handler function. The interrupt core would
 * call this for each of the mailboxes the interrupt is configured.
 */
static irqreturn_t mbox_interrupt(int irq, void *p)
{
	struct mailbox *mbox = p;

	if (is_mbox_irq(mbox, IRQ_TX))
		__mbox_tx_interrupt(mbox);

	if (is_mbox_irq(mbox, IRQ_RX))
		__mbox_rx_interrupt(mbox);

	return IRQ_HANDLED;
}

/*
 * Helper function to allocate a mailbox queue object. This function
 * also creates either or both of the work-queue or tasklet to
 * deal with processing of messages on the kfifo associated with
 * the mailbox queue object.
 */
static struct mailbox_queue *mbox_queue_alloc(struct mailbox *mbox,
		void (*work) (struct work_struct *),
		void (*tasklet)(unsigned long))
{
	struct mailbox_queue *mq;

	mq = kzalloc(sizeof(struct mailbox_queue), GFP_KERNEL);
	if (!mq)
		return NULL;

	spin_lock_init(&mq->lock);
	mutex_init(&mq->mlock);

	if (kfifo_alloc(&mq->fifo, mbox_kfifo_size, GFP_KERNEL))
		goto error;

	if (work)
		INIT_WORK(&mq->work, work);

	if (tasklet)
		tasklet_init(&mq->tasklet, tasklet, (unsigned long)mbox);
	return mq;
error:
	kfree(mq);
	return NULL;
}

/*
 * Helper function to free a mailbox queue object.
 */
static void mbox_queue_free(struct mailbox_queue *q)
{
	kfifo_free(&q->fifo);
	kfree(q);
}

/*
 * Helper function to initialize a mailbox. This function creates
 * the mailbox queue objects associated with the mailbox h/w channel
 * and plugs-in the interrupt associated with the mailbox.
 */
static int mailbox_startup(struct mailbox *mbox)
{
	int ret = 0;
	struct mailbox_queue *mq;
	struct mailbox_device *mdev = mbox->parent;

	mutex_lock(&mdev->cfg_lock);
	if (!mdev->cfg_count++) {
		if (mdev->dev_ops && mdev->dev_ops->startup) {
			ret = mdev->dev_ops->startup(mdev, NULL);
			if (unlikely(ret))
				goto fail_startup;
		} else {
			pr_warn("nothing to do for mailbox device startup\n");
		}
	}

	if (!mbox->use_count++) {
		if (likely(mbox->ops->startup)) {
			ret = mbox->ops->startup(mbox);
			if (unlikely(ret))
				goto fail_mbox_startup;
		} else {
			pr_warn("nothing to do for mailbox startup\n");
		}

		mq = mbox_queue_alloc(mbox, NULL, mbox_tx_tasklet);
		if (!mq) {
			ret = -ENOMEM;
			goto fail_alloc_txq;
		}
		mbox->txq = mq;

		mq = mbox_queue_alloc(mbox, mbox_rx_work, NULL);
		if (!mq) {
			ret = -ENOMEM;
			goto fail_alloc_rxq;
		}
		mbox->rxq = mq;
		mq->mbox = mbox;
		ret = request_irq(mbox->irq, mbox_interrupt,
					mbox->irq_flags, mbox->name, mbox);
		if (unlikely(ret)) {
			pr_err("failed to register mailbox interrupt:%d\n",
					ret);
			goto fail_request_irq;
		}

		mailbox_enable_irq(mbox, IRQ_RX);
	}
	mutex_unlock(&mdev->cfg_lock);
	return 0;

fail_request_irq:
	mbox_queue_free(mbox->rxq);
fail_alloc_rxq:
	mbox_queue_free(mbox->txq);
fail_alloc_txq:
	if (mbox->ops->shutdown)
		mbox->ops->shutdown(mbox);
	mbox->use_count--;
fail_mbox_startup:
	if ((mdev->cfg_count == 1) && mdev->dev_ops &&
		mdev->dev_ops->shutdown)
		mdev->dev_ops->shutdown(mdev);
fail_startup:
	mdev->cfg_count--;
	mutex_unlock(&mdev->cfg_lock);
	return ret;
}

/*
 * Helper function to de-initialize a mailbox
 */
static void mailbox_fini(struct mailbox *mbox)
{
	struct mailbox_device *mdev = mbox->parent;

	mutex_lock(&mdev->cfg_lock);
	if (!--mbox->use_count) {
		mailbox_disable_irq(mbox, IRQ_RX);
		free_irq(mbox->irq, mbox);
		tasklet_kill(&mbox->txq->tasklet);
		flush_work(&mbox->rxq->work);
		mbox_queue_free(mbox->txq);
		mbox_queue_free(mbox->rxq);
		if (likely(mbox->ops->shutdown))
			mbox->ops->shutdown(mbox);
	}

	if (!--mdev->cfg_count) {
		if (mdev->dev_ops && mdev->dev_ops->shutdown)
			mdev->dev_ops->shutdown(mdev);
	}

	mutex_unlock(&mdev->cfg_lock);
}

/*
 * Helper function to find a mailbox. It is currently assumed that all the
 * mailbox names are unique among all the mailbox devices. This can be
 * easily extended if only a particular mailbox device is to searched.
 */
static struct mailbox *mailbox_device_find(struct mailbox_device *mdev,
					   const char *mbox_name)
{
	struct mailbox *_mbox, *mbox = NULL;
	struct mailbox **mboxes = mdev->mboxes;
	int i;

	if (!mboxes)
		return NULL;

	for (i = 0; (_mbox = mboxes[i]); i++) {
		if (!strcmp(_mbox->name, mbox_name)) {
			mbox = _mbox;
			break;
		}
	}
	return mbox;
}

/**
 * mailbox_get() - acquire a mailbox
 * @name: name of the mailbox to acquire
 * @nb: notifier block to be invoked on received messages
 *
 * This API is called by a client user to use a mailbox. The returned handle
 * needs to be used by the client for invoking any other mailbox API. Any
 * message received on the mailbox is delivered to the client through the
 * 'nb' notifier. There are currently no restrictions on multiple clients
 * acquiring the same mailbox - the same message is delivered to each of the
 * clients through their respective notifiers.
 *
 * The function ensures that the mailbox is put into an operational state
 * before the function returns.
 *
 * Returns a usable mailbox handle on success, or NULL otherwise
 */
struct mailbox *mailbox_get(const char *name, struct notifier_block *nb)
{
	struct mailbox *mbox = NULL;
	struct mailbox_device *mdev;
	int ret;

	mutex_lock(&mailbox_devices_lock);
	list_for_each_entry(mdev, &mailbox_devices, next) {
		mbox = mailbox_device_find(mdev, name);
		if (mbox)
			break;
	}
	mutex_unlock(&mailbox_devices_lock);

	if (!mbox)
		return ERR_PTR(-ENOENT);

	if (nb)
		blocking_notifier_chain_register(&mbox->notifier, nb);

	ret = mailbox_startup(mbox);
	if (ret) {
		blocking_notifier_chain_unregister(&mbox->notifier, nb);
		return ERR_PTR(-ENODEV);
	}

	return mbox;
}
EXPORT_SYMBOL(mailbox_get);

/**
 * mailbox_put() - release a mailbox
 * @mbox: handle to the acquired mailbox
 * @nb: notifier block used while acquiring the mailbox
 *
 * This API is to be called by a client user once it is done using the
 * mailbox. The particular user's notifier function is removed from the
 * notifier list of received messages on this mailbox. It also undoes
 * any h/w configuration done during the acquisition of the mailbox.
 *
 * No return value
 */
void mailbox_put(struct mailbox *mbox, struct notifier_block *nb)
{
	if (nb)
		blocking_notifier_chain_unregister(&mbox->notifier, nb);
	mailbox_fini(mbox);
}
EXPORT_SYMBOL(mailbox_put);

/**
 * mailbox_device_alloc() - allocate a mailbox device object
 * @dev: reference device pointer of the h/w mailbox block
 * @dev_ops: h/w mailbox device specific function ops
 * @len: length of the driver-specific private structure
 *
 * This API is called by a mailbox driver implementor to create a common
 * mailbox device object. The mailbox core maintains the list of all the
 * mailbox devices. The returned handle needs to be used while registering
 * all the mailboxes within the h/w block with the mailbox core.
 *
 * Returns a mailbox device handle on success, or NULL otherwise
 */
struct mailbox_device *mailbox_device_alloc(struct device *dev,
					    struct mailbox_device_ops *dev_ops,
					    int len)
{
	struct mailbox_device *mdev;

	if (!dev)
		return NULL;

	mdev = kzalloc(sizeof(*mdev) + len, GFP_KERNEL);
	if (!mdev)
		return NULL;

	mutex_init(&mdev->cfg_lock);
	mdev->dev = dev;
	mdev->dev_ops = dev_ops;
	if (len)
		mdev->priv = &mdev[1];
	mutex_lock(&mailbox_devices_lock);
	list_add(&mdev->next, &mailbox_devices);
	mutex_unlock(&mailbox_devices_lock);

	return mdev;
}
EXPORT_SYMBOL(mailbox_device_alloc);

/**
 * mailbox_device_free() - free a  mailbox device object
 * @mdev: mailbox device object to be freed
 *
 * This API is called by a mailbox driver implementor to delete the common
 * mailbox device object, it had created. This device is freed and removed
 * from the list of all mailbox devices that the mailbox core maintains.
 */
void mailbox_device_free(struct mailbox_device *mdev)
{
	mutex_lock(&mailbox_devices_lock);
	list_del(&mdev->next);
	mutex_unlock(&mailbox_devices_lock);

	kfree(mdev);
}
EXPORT_SYMBOL(mailbox_device_free);

static struct class mailbox_class = { .name = "mbox", };

/**
 * mailbox_register() - register the list of mailboxes
 * @mdev: mailbox device handle that the mailboxes need to be registered
 *	  with
 * @list: list of mailboxes associated with the mailbox device
 *
 * This API is to be called by individual mailbox driver implementations
 * for registering the set of mailboxes contained in a h/w communication
 * block with the mailbox core. Each of the mailbox represents a h/w
 * communication channel, contained within the h/w communication block or ip.
 *
 * An associated device is also created for each of the mailboxes.
 *
 * Return 0 on success, or a failure code otherwise
 */
int mailbox_register(struct mailbox_device *mdev, struct mailbox **list)
{
	int ret;
	int i;
	struct mailbox **mboxes;

	if (!mdev || !list)
		return -EINVAL;

	if (mdev->mboxes)
		return -EBUSY;

	mboxes = mdev->mboxes = list;
	for (i = 0; mboxes[i]; i++) {
		struct mailbox *mbox = mboxes[i];
		mbox->parent = mdev;
		mbox->dev = device_create(&mailbox_class,
				mdev->dev, 0, mbox, "%s", mbox->name);
		if (IS_ERR(mbox->dev)) {
			ret = PTR_ERR(mbox->dev);
			goto err_out;
		}

		BLOCKING_INIT_NOTIFIER_HEAD(&mbox->notifier);
	}
	return 0;

err_out:
	while (i--)
		device_unregister(mboxes[i]->dev);
	mdev->mboxes = NULL;
	return ret;
}
EXPORT_SYMBOL(mailbox_register);

/**
 * mailbox_unregister() - unregister the list of mailboxes
 * @mdev: parent mailbox device handle containing the mailboxes that need
 *	  to be unregistered
 *
 * This API is to be called by individual mailbox driver implementations
 * for unregistering the set of mailboxes contained in a h/w communication
 * block. Once unregistered, these mailboxes are not available for any
 * client users/drivers.
 *
 * Return 0 on success, or a failure code otherwise
 */
int mailbox_unregister(struct mailbox_device *mdev)
{
	int i;
	struct mailbox **mboxes;

	if (!mdev || !mdev->mboxes)
		return -EINVAL;

	mboxes = mdev->mboxes;
	for (i = 0; mboxes[i]; i++) {
		device_unregister(mboxes[i]->dev);
		mboxes[i]->parent = NULL;
	}
	mdev->mboxes = NULL;
	return 0;
}
EXPORT_SYMBOL(mailbox_unregister);

static int __init mailbox_init(void)
{
	int err;

	err = class_register(&mailbox_class);
	if (err)
		return err;

	/* kfifo size sanity check: alignment and minimal size */
	mbox_kfifo_size = ALIGN(mbox_kfifo_size, sizeof(struct mailbox_msg));
	mbox_kfifo_size = max_t(unsigned int, mbox_kfifo_size,
			sizeof(struct mailbox_msg) + CONFIG_MBOX_DATA_SIZE);
	return 0;
}
subsys_initcall(mailbox_init);

static void __exit mailbox_exit(void)
{
	class_unregister(&mailbox_class);
}
module_exit(mailbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mailbox framework: interrupt driven messaging");
MODULE_AUTHOR("Toshihiro Kobayashi");
MODULE_AUTHOR("Hiroshi DOYU");
