/*
 * mailbox: interprocessor communication module
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MAILBOX_INTERNAL_H
#define MAILBOX_INTERNAL_H

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/mailbox.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

/**
 * struct mailbox_ops - function ops specific to a mailbox implementation
 * @startup: the startup function, essential for making the mailbox active.
 *	     This will be called when the first client acquires the mailbox.
 * @shutdown: the shutdown function, essential for making the mailbox inactive
 *	      after usage. This will be called when the last client releases
 *	      the mailbox.
 * @read: read the h/w transport payload and package it in the mailbox message
 *	  packet for consumption by clients. This hook provides the mailbox
 *	  core to read all the available messages upon a Rx interrupt and buffer
 *	  them. The messages are delivered to the clients in a workqueue.
 * @write: send a mailbox message packet on the h/w transport channel. The
 *	   individual drivers are responsible for the parsing of the pdata
 *	   pointer in the mailbox_msg packet and configuring the h/w
 *	   accordingly.
 * @empty: check if the h/w Rx transport has more messages. The function should
 *	   return 0 if there are no more messages to be read from the transport,
 *	   and non-zero if there are available messages.
 * @poll_for_space: check if the h/w Tx transport is busy. This hook should
 *	   return non-zero if the h/w Tx transport is busy, and 0 when the h/w
 *	   communication channel is free.
 * @enable_irq: This hook allows the mailbox core to allow a specific Rx or Tx
 *		interrupt signal to interrupt the processor, based on its state
 *		machine.
 * @disable_irq: This hooks allows the mailbox core to disable a specific Rx or
 *		 Tx interrupt signal from interrupting the processor, based on
 *		 its state machine.
 * @ack_irq: acknowledge the Tx or Rx interrupt signal internal to the mailbox.
 *	     This allows the h/w communication block to clear any internal
 *	     interrupt source status registers.
 * @is_irq: check if a particular Tx or Rx interrupt signal on the corresponding
 *	    mailbox is set. This hook is used by the mailbox core to process the
 *	    interrupt accordingly.
 * @save_ctx: Called by a client or the mailbox core to allow the individual
 *	      driver implementation to save the context of the mailbox registers
 *	      before the domain containing the h/w communication block can be
 *	      put into a low-power state.
 * @restore_ctx: Called by a client or the mailbox core to allow the individual
 *	      driver implementation to restore the context of the mailbox
 *	      registers after the domain containing the h/w communication block
 *	      is powered back to active state.
 */
struct mailbox_ops {
	int		(*startup)(struct mailbox *mbox);
	void		(*shutdown)(struct mailbox *mbox);
	/* mailbox access */
	void		(*read)(struct mailbox *mbox, struct mailbox_msg *msg);
	int		(*write)(struct mailbox *mbox, struct mailbox_msg *msg);
	int		(*empty)(struct mailbox *mbox);
	int		(*poll_for_space)(struct mailbox *mbox);
	int		(*needs_flush)(struct mailbox *mbox);
	void		(*readback)(struct mailbox *mbox,
					struct mailbox_msg *msg);
	/* irq */
	void		(*enable_irq)(struct mailbox *mbox, mailbox_irq_t irq);
	void		(*disable_irq)(struct mailbox *mbox, mailbox_irq_t irq);
	void		(*ack_irq)(struct mailbox *mbox, mailbox_irq_t irq);
	int		(*is_irq)(struct mailbox *mbox, mailbox_irq_t irq);
	/* context */
	void		(*save_ctx)(struct mailbox *mbox);
	void		(*restore_ctx)(struct mailbox *mbox);
};

/**
 * struct mailbox_queue - A queue object used for buffering messages
 * @lock: a spinlock providing synchronization in atomic context
 * @mlock: a mutex providing synchronization in thread context
 * @fifo: a kfifo object for buffering the messages. The size of the kfifo is
 *	  is currently configured per driver implementation at build time using
 *	  kernel menu configuration. The usage of the kfifo depends on whether
 *	  the queue object is for Rx or Tx. For Tx, a message is buffered into
 *	  the kfifo if the h/w transport is busy, and is taken out when the h/w
 *	  signals Tx readiness. For Rx, the messages are buffered into the kfifo
 *	  in the bottom-half processing of a Rx interrupt, and taken out during
 *	  the top-half processing.
 * @work: a workqueue object for scheduling top-half processing of messages
 * @tasklet: a tasklet object for processing messages in an atomic context
 * @mbox: reference to the containing parent mailbox
 * full: indicates the status of the fifo, and is set to true when there is no
 *	 room in the fifo.
 */
struct mailbox_queue {
	spinlock_t		lock;
	struct mutex		mlock;
	struct kfifo		fifo;
	struct work_struct	work;
	struct tasklet_struct	tasklet;
	struct mailbox		*mbox;
	bool full;
};

/**
 * struct mailbox - the base object describing a h/w communication channel.
 *	  there can be more than one object in a h/w communication block
 * @name: a unique name for the mailbox object. Client users acquire a
 *	  mailbox object using this name
 * @id: an unique integer identifier for the mailbox object belonging
 *	  to the same h/w communication block
 * @irq: IRQ number that the mailbox uses to interrupt the host processor.
 *	  the same IRQ number may be shared between different mailboxes
 * @irqflags: flags to be used when requesting the irq. The h/w block may
 *	      impose/require specific irq flags
 * @txq: the mailbox queue object pertaining to Tx
 * @rxq: the mailbox queue object pertaining to Rx
 * @ops: function ops specific to the mailbox
 * @dev: the device pointer representing the mailbox object
 * @priv: a private structure specific to the driver implementation, this will
 *	  not be touched by the mailbox core
 * @use_count: number of current references to the mailbox, useful in
 *	  controlling the mailbox state
 * @parent: back reference to the containing parent mailbox device object
 * @notifier: notifier chain of clients, to which a received message is
 *	  communicated
 */
struct mailbox {
	const char		*name;
	unsigned int		id;
	unsigned int		irq;
	unsigned long		irq_flags;
	struct mailbox_queue	*txq, *rxq;
	struct mailbox_ops	*ops;
	struct device		*dev;
	void			*priv;
	int			use_count;
	struct mailbox_device	*parent;
	struct blocking_notifier_head	notifier;
};

/*
 * mailbox objects registration and de-registration functions with the
 * mailbox core.
 */
int mailbox_register(struct mailbox_device *mdev, struct mailbox **);
int mailbox_unregister(struct mailbox_device *mdev);

#endif /* MAILBOX_INTERNAL_H */
