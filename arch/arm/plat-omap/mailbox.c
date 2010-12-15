/*
 * OMAP mailbox driver
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/notifier.h>

#include <plat/mailbox.h>

static struct workqueue_struct *mboxd;
static struct omap_mbox *mboxes;
static DEFINE_MUTEX(mboxes_lock);

static int mbox_configured;

/* Mailbox FIFO handle functions */
static inline mbox_msg_t mbox_fifo_read(struct omap_mbox *mbox)
{
	return mbox->ops->fifo_read(mbox);
}
static inline void mbox_fifo_write(struct omap_mbox *mbox, mbox_msg_t msg)
{
	mbox->ops->fifo_write(mbox, msg);
}
static inline int mbox_fifo_empty(struct omap_mbox *mbox)
{
	return mbox->ops->fifo_empty(mbox);
}
static inline int mbox_fifo_full(struct omap_mbox *mbox)
{
	return mbox->ops->fifo_full(mbox);
}

/* Mailbox IRQ handle functions */
static inline void ack_mbox_irq(struct omap_mbox *mbox, omap_mbox_irq_t irq)
{
	if (mbox->ops->ack_irq)
		mbox->ops->ack_irq(mbox, irq);
}
static inline int is_mbox_irq(struct omap_mbox *mbox, omap_mbox_irq_t irq)
{
	return mbox->ops->is_irq(mbox, irq);
}

/*
 * message sender
 */
static int __mbox_poll_for_space(struct omap_mbox *mbox)
{
	int ret = 0, i = 1000;

	while (mbox_fifo_full(mbox)) {
		if (--i == 0)
			return -1;
		udelay(1);
		printk(KERN_ERR "Mailbox FIFO full %d\n", i);
	}
	return ret;
}


int omap_mbox_msg_send(struct omap_mbox *mbox, mbox_msg_t msg)
{
	struct omap_mbox_queue *mq = mbox->txq;
	int ret = 0, len;

	spin_lock_bh(&mq->lock);
	if (kfifo_avail(&mq->fifo) < sizeof(msg)) {
		ret = -ENOMEM;
		goto out;
	}

	len = kfifo_in(&mq->fifo, (unsigned char *)&msg, sizeof(msg));
	if (unlikely(len != sizeof(msg))) {
		pr_err("%s: kfifo_in anomaly\n", __func__);
		ret = -ENOMEM;
	}

	tasklet_schedule(&mbox->txq->tasklet);

out:
	spin_unlock_bh(&mq->lock);
	return ret;
}
EXPORT_SYMBOL(omap_mbox_msg_send);

static void mbox_tx_tasklet(unsigned long tx_data)
{
	struct omap_mbox *mbox = (struct omap_mbox *)tx_data;
	struct omap_mbox_queue *mq = mbox->txq;
	mbox_msg_t msg;
	int ret;

	spin_lock(&mq->lock);
	while (kfifo_len(&mq->fifo)) {
		if (__mbox_poll_for_space(mbox)) {
			omap_mbox_enable_irq(mbox, IRQ_TX);
			break;
		}

		ret = kfifo_out(&mq->fifo, (unsigned char *)&msg,
								sizeof(msg));
		if (unlikely(ret != sizeof(msg)))
			pr_err("%s: kfifo_out anomaly\n", __func__);

		mbox_fifo_write(mbox, msg);


	}
	spin_unlock(&mq->lock);
}

/*
 * Message receiver(workqueue)
 */
static void mbox_rx_work(struct work_struct *work)
{
	struct omap_mbox_queue *mq =
			container_of(work, struct omap_mbox_queue, work);
	mbox_msg_t msg;
	int len;

	while (kfifo_len(&mq->fifo) >= sizeof(msg)) {
		len = kfifo_out(&mq->fifo, (unsigned char *)&msg, sizeof(msg));
		if (unlikely(len != sizeof(msg)))
			pr_err("%s: kfifo_out anomaly detected\n", __func__);

		blocking_notifier_call_chain(&mq->mbox->notifier, len,
							(void *)msg);
		spin_lock_irq(&mq->lock);
		if (mq->full) {
			mq->full = false;
			omap_mbox_enable_irq(mq->mbox, IRQ_RX);
		}
		spin_unlock_irq(&mq->lock);
	}
}

/*
 * Mailbox interrupt handler
 */
static void __mbox_tx_interrupt(struct omap_mbox *mbox)
{
	struct omap_mbox **tmp;
	struct omap_mbox *mbox_curr;

	tmp = &mboxes;
	while (*tmp) {
		mbox_curr = *tmp;
		if (!mbox_fifo_full(mbox_curr)) {
			omap_mbox_disable_irq(mbox_curr, IRQ_TX);
			ack_mbox_irq(mbox_curr, IRQ_TX);
			tasklet_schedule(&mbox_curr->txq->tasklet);
		}
		tmp = &(*tmp)->next;
	}
}

static void __mbox_rx_interrupt(struct omap_mbox *mbox)
{
	struct omap_mbox_queue *mq;
	mbox_msg_t msg;
	int len;
	struct omap_mbox **tmp;
	struct omap_mbox *mbox_curr;
	bool msg_rx;

	tmp = &mboxes;
	while (*tmp) {
		mbox_curr = *tmp;
		mq = mbox_curr->rxq;
		msg_rx = false;
		while (!mbox_fifo_empty(mbox_curr)) {
			msg_rx = true;
			if (unlikely(kfifo_avail(&mq->fifo) < sizeof(msg))) {
				omap_mbox_disable_irq(mbox_curr, IRQ_RX);
				mq->full = true;
				goto nomem;
			}

			msg = mbox_fifo_read(mbox_curr);

			len = kfifo_in(&mq->fifo, (unsigned char *)&msg,
								sizeof(msg));
			if (unlikely(len != sizeof(msg)))
				pr_err("%s: kfifo_in anomaly detected\n",
								__func__);
			if (mbox->ops->type == OMAP_MBOX_TYPE1)
				break;
		}
		/* no more messages in the fifo. clear IRQ source. */
		if (msg_rx)
			ack_mbox_irq(mbox_curr, IRQ_RX);
nomem:
		queue_work(mboxd, &mbox->rxq->work);

		tmp = &(*tmp)->next;
	}
}

static irqreturn_t mbox_interrupt(int irq, void *p)
{
	struct omap_mbox *mbox = p;

	if (is_mbox_irq(mbox, IRQ_TX))
		__mbox_tx_interrupt(mbox);

	if (is_mbox_irq(mbox, IRQ_RX))
		__mbox_rx_interrupt(mbox);

	return IRQ_HANDLED;
}

static struct omap_mbox_queue *mbox_queue_alloc(struct omap_mbox *mbox,
					void (*work) (struct work_struct *),
					void (*tasklet)(unsigned long))
{
	struct omap_mbox_queue *mq;

	mq = kzalloc(sizeof(struct omap_mbox_queue), GFP_KERNEL);
	if (!mq)
		return NULL;

	spin_lock_init(&mq->lock);

	if (kfifo_alloc(&mq->fifo, MBOX_KFIFO_SIZE, GFP_KERNEL))
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

static void mbox_queue_free(struct omap_mbox_queue *q)
{
	kfifo_free(&q->fifo);
	kfree(q);
}

static int omap_mbox_startup(struct omap_mbox *mbox)
{
	int ret = 0;
	struct omap_mbox_queue *mq;

	mutex_lock(&mboxes_lock);
	if (!mbox_configured++) {
		if (likely(mbox->ops->startup)) {
			ret = mbox->ops->startup(mbox);
			if (unlikely(ret)) {
				mbox_configured--;
				mutex_unlock(&mboxes_lock);
				return ret;
			}
		}
	}

	if (!mbox->use_count++) {
		ret = request_irq(mbox->irq, mbox_interrupt, IRQF_SHARED,
					mbox->name, mbox);
		if (unlikely(ret)) {
			printk(KERN_ERR
			"failed to register mailbox interrupt:%d\n", ret);
			goto fail_request_irq;
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
	}
	mutex_unlock(&mboxes_lock);
	return 0;

 fail_alloc_rxq:
	mbox_queue_free(mbox->txq);
 fail_alloc_txq:
	free_irq(mbox->irq, mbox);
 fail_request_irq:
	if (likely(mbox->ops->shutdown))
		mbox->ops->shutdown(mbox);
	mbox_configured--;
	mbox->use_count--;
	mutex_unlock(&mboxes_lock);
	return ret;
}

static void omap_mbox_fini(struct omap_mbox *mbox)
{
	mutex_lock(&mboxes_lock);

	if (!--mbox->use_count) {
		omap_mbox_disable_irq(mbox, IRQ_RX);
		omap_mbox_disable_irq(mbox, IRQ_TX);
		flush_work(&mbox->rxq->work);
		flush_work(&mbox->txq->work);
		mbox_queue_free(mbox->txq);
		mbox_queue_free(mbox->rxq);
	}

	if (likely(mbox->ops->shutdown)) {
		if (!--mbox_configured) {
			free_irq(mbox->irq, mbox);
			mbox->ops->shutdown(mbox);
		}
	}

	mutex_unlock(&mboxes_lock);
}

static struct omap_mbox **find_mboxes(const char *name)
{
	struct omap_mbox **p;

	for (p = &mboxes; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, name) == 0)
			break;
	}

	return p;
}

struct omap_mbox *omap_mbox_get(const char *name, struct notifier_block *nb)
{
	struct omap_mbox *mbox;
	int ret;

	mutex_lock(&mboxes_lock);
	mbox = *(find_mboxes(name));
	if (mbox == NULL) {
		mutex_unlock(&mboxes_lock);
		return ERR_PTR(-ENOENT);
	}

	mutex_unlock(&mboxes_lock);

	ret = omap_mbox_startup(mbox);
	if (ret)
		return ERR_PTR(-ENODEV);
	if (nb)
		blocking_notifier_chain_register(&mbox->notifier, nb);

	return mbox;
}
EXPORT_SYMBOL(omap_mbox_get);

void omap_mbox_put(struct omap_mbox *mbox, struct notifier_block *nb)
{
	if (nb)
		blocking_notifier_chain_unregister(&mbox->notifier, nb);
	omap_mbox_fini(mbox);
}
EXPORT_SYMBOL(omap_mbox_put);

int omap_mbox_register(struct device *parent, struct omap_mbox *mbox)
{
	int ret = 0;
	struct omap_mbox **tmp;

	if (!mbox)
		return -EINVAL;
	if (mbox->next)
		return -EBUSY;

	mutex_lock(&mboxes_lock);
	tmp = find_mboxes(mbox->name);
	if (*tmp) {
		ret = -EBUSY;
		mutex_unlock(&mboxes_lock);
		goto err_find;
	}
	*tmp = mbox;
	BLOCKING_INIT_NOTIFIER_HEAD(&mbox->notifier);
	mutex_unlock(&mboxes_lock);

	return 0;

err_find:
	return ret;
}
EXPORT_SYMBOL(omap_mbox_register);

int omap_mbox_unregister(struct omap_mbox *mbox)
{
	struct omap_mbox **tmp;

	mutex_lock(&mboxes_lock);
	tmp = &mboxes;
	while (*tmp) {
		if (mbox == *tmp) {
			*tmp = mbox->next;
			mbox->next = NULL;
			mutex_unlock(&mboxes_lock);
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	mutex_unlock(&mboxes_lock);

	return -EINVAL;
}
EXPORT_SYMBOL(omap_mbox_unregister);

static int __init omap_mbox_init(void)
{
	mboxd = create_singlethread_workqueue("mboxd");
	if (!mboxd)
		return -ENOMEM;

	return 0;
}
module_init(omap_mbox_init);

static void __exit omap_mbox_exit(void)
{
	destroy_workqueue(mboxd);
}
module_exit(omap_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("omap mailbox: interrupt driven messaging");
MODULE_AUTHOR("Toshihiro Kobayashi");
MODULE_AUTHOR("Hiroshi DOYU");
