/*
 * mailbox: interprocessor communication module
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MAILBOX_H
#define MAILBOX_H

/* forward declaration for clients */
struct mailbox;

/* interrupt direction identifiers */
typedef int __bitwise mailbox_irq_t;
#define IRQ_TX ((__force mailbox_irq_t) 1)
#define IRQ_RX ((__force mailbox_irq_t) 2)

/**
 * struct mailbox_device - device structure for storing h/w mailbox block
 * @dev:	reference device pointer of the h/w mailbox block
 * @cfg_lock:	a configureation mutex lock used for protecting the mailbox
 *		device configuration operations
 * @cfg_count:	number of current references to the mailbox device, useful
 *		in configuring the mailbox device.
 * @dev_ops:	mailbox device ops for configuring a mailbox device, these can
 *		be optional if the particular mailbox driver doesn't need to
 *		perform any configuration
 * @mboxes:	array of containing mailboxes within the h/w mailbox block
 * @next:	list node
 * @priv:	private pointer specific to a mailbox driver implementation,
 *		used for storing any driver-specific mailbox device structure
 */
struct mailbox_device {
	struct device *dev;
	struct mutex cfg_lock;
	int cfg_count;
	struct mailbox_device_ops *dev_ops;
	struct mailbox **mboxes;
	struct list_head next;
	void *priv;
};

/**
 * struct mailbox_device_ops - function ops specific to a mailbox device
 * @startup: the startup function, essential for making the mailbox device
 *	     block active. This will be called when any mailbox within the
 *	     mailbox device is  acquired for the first time.
 * @shutdown: the shutdown function, essential for making the mailbox device
 *	      block inactive after usage. This will be called when the last
 *	      of the mailboxes in use is released.
 */
struct mailbox_device_ops {
	int	(*startup)(struct mailbox_device *mdev, void *data);
	void	(*shutdown)(struct mailbox_device *mdev);
};

/**
 * struct mailbox_msg - Main message structure used by clients
 * @size:	Size of the message data contained in pdata.
 * @header:	A header element, if any, for a message. Usage
 *		of this header element is upto the respective
 *		driver implementation (TODO: this is somewhat
 *		driver-specific, eliminate this)
 * @pdata:	Null terminated array of links.
 */
struct mailbox_msg {
	int		size;
	u32		header;
	void		*pdata;
};

#define MAILBOX_FILL_MSG(_msg, _header, _pdata, _size) { \
	_msg.header = _header; \
	_msg.pdata = (void *)_pdata; \
	_msg.size = _size; \
}

/* client api for message transmission */
int mailbox_msg_send(struct mailbox *, struct mailbox_msg *msg);
struct mailbox_msg *mailbox_msg_send_receive_no_irq(struct mailbox *,
		struct mailbox_msg *msg);
int mailbox_msg_send_no_irq(struct mailbox *, struct mailbox_msg *msg);
int mailbox_empty_tx(struct mailbox *mbox);

/* client api for acquiring and releasing a mailbox */
struct mailbox *mailbox_get(const char *, struct notifier_block *nb);
void mailbox_put(struct mailbox *mbox, struct notifier_block *nb);

/*
 * Temporary exported functions to maintain TI DSP/Bridge functionality.
 * These will be deprecated, new clients should not use these.
 */
void mailbox_save_ctx(struct mailbox *mbox);
void mailbox_restore_ctx(struct mailbox *mbox);
void mailbox_enable_irq(struct mailbox *mbox, mailbox_irq_t irq);
void mailbox_disable_irq(struct mailbox *mbox, mailbox_irq_t irq);

/* driver api for managing the mailbox h/w device block */
struct mailbox_device *mailbox_device_alloc(struct device *dev,
					    struct mailbox_device_ops *dev_ops,
					    int len);
void mailbox_device_free(struct mailbox_device *mdev);

#endif /* MAILBOX_H */
