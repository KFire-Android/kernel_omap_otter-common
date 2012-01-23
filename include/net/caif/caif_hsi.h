/*
 * Copyright (C) ST-Ericsson AB 2010
 * Contact: Sjur Brendeland / sjur.brandeland@stericsson.com
 * Author:  Daniel Martensson / daniel.martensson@stericsson.com
 *	    Dmitry.Tarnyagin  / dmitry.tarnyagin@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CAIF_HSI_H_
#define CAIF_HSI_H_

#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <net/caif/caif_layer.h>
#include <net/caif/caif_device.h>
#include <linux/atomic.h>

/*
 * Maximum number of CAIF frames that can reside in the same HSI frame.
 */
#define CFHSI_MAX_PKTS 15

/*
 * Maximum number of bytes used for the frame that can be embedded in the
 * HSI descriptor.
 */
#define CFHSI_MAX_EMB_FRM_SZ 96

/*
 * Decides if HSI buffers should be prefilled with 0xFF pattern for easier
 * debugging. Both TX and RX buffers will be filled before the transfer.
 */
#define CFHSI_DBG_PREFILL		0

/* Structure describing a HSI packet descriptor. */
#pragma pack(1) /* Byte alignment. */
struct cfhsi_desc {
	u8 header;
	u8 offset;
	u16 cffrm_len[CFHSI_MAX_PKTS];
	u8 emb_frm[CFHSI_MAX_EMB_FRM_SZ];
};
#pragma pack() /* Default alignment. */

/* Size of the complete HSI packet descriptor. */
#define CFHSI_DESC_SZ (sizeof(struct cfhsi_desc))

/*
 * Size of the complete HSI packet descriptor excluding the optional embedded
 * CAIF frame.
 */
#define CFHSI_DESC_SHORT_SZ (CFHSI_DESC_SZ - CFHSI_MAX_EMB_FRM_SZ)

/*
 * Maximum bytes transferred in one transfer.
 */
#define CFHSI_MAX_CAIF_FRAME_SZ 4096

#define CFHSI_MAX_PAYLOAD_SZ (CFHSI_MAX_PKTS * CFHSI_MAX_CAIF_FRAME_SZ)

/* Size of the complete HSI TX buffer. */
#define CFHSI_BUF_SZ_TX (CFHSI_DESC_SZ + CFHSI_MAX_PAYLOAD_SZ)

/* Size of the complete HSI RX buffer. */
#define CFHSI_BUF_SZ_RX (2 * CFHSI_DESC_SZ + CFHSI_MAX_PAYLOAD_SZ)

/* Bitmasks for the HSI descriptor. */
#define CFHSI_PIGGY_DESC		(0x01 << 7)

#define CFHSI_TX_STATE_IDLE			0
#define CFHSI_TX_STATE_XFER			1

#define CFHSI_RX_STATE_DESC			0
#define CFHSI_RX_STATE_PAYLOAD			1

/* Bitmasks for power management. */
#define CFHSI_WAKE_UP				0
#define CFHSI_WAKE_UP_ACK			1
#define CFHSI_WAKE_DOWN_ACK			2
#define CFHSI_AWAKE				3
#define CFHSI_WAKELOCK_HELD			4
#define CFHSI_SHUTDOWN				5
#define CFHSI_FLUSH_FIFO			6

#ifndef CFHSI_INACTIVITY_TOUT
#define CFHSI_INACTIVITY_TOUT			(1 * HZ)
#endif /* CFHSI_INACTIVITY_TOUT */

#ifndef CFHSI_WAKE_TOUT
#define CFHSI_WAKE_TOUT			(3 * HZ)
#endif /* CFHSI_WAKE_TOUT */

#ifndef CFHSI_MAX_RX_RETRIES
#define CFHSI_MAX_RX_RETRIES		(10 * HZ)
#endif

#ifndef CFHSI_RX_GRACE_PERIOD
#define CFHSI_RX_GRACE_PERIOD		(1 * HZ)
#endif

/* Structure implemented by the CAIF HSI driver. */
struct cfhsi_drv {
	void (*tx_done_cb) (struct cfhsi_drv *drv);
	void (*rx_done_cb) (struct cfhsi_drv *drv);
	void (*wake_up_cb) (struct cfhsi_drv *drv);
	void (*wake_down_cb) (struct cfhsi_drv *drv);
};

/* Structure implemented by HSI device. */
struct cfhsi_dev {
	int (*cfhsi_up) (struct cfhsi_dev *dev);
	int (*cfhsi_down) (struct cfhsi_dev *dev);
	int (*cfhsi_tx) (u8 *ptr, int len, struct cfhsi_dev *dev);
	int (*cfhsi_rx) (u8 *ptr, int len, struct cfhsi_dev *dev);
	int (*cfhsi_wake_up) (struct cfhsi_dev *dev);
	int (*cfhsi_wake_down) (struct cfhsi_dev *dev);
	int (*cfhsi_get_peer_wake) (struct cfhsi_dev *dev, bool *status);
	int (*cfhsi_fifo_occupancy)(struct cfhsi_dev *dev, size_t *occupancy);
	int (*cfhsi_rx_cancel)(struct cfhsi_dev *dev);
	struct cfhsi_drv *drv;
};

/* Structure holds status of received CAIF frames processing */
struct cfhsi_rx_state {
	int state;
	int nfrms;
	int pld_len;
	int retries;
	bool piggy_desc;
};

/* Structure implemented by CAIF HSI drivers. */
struct cfhsi {
	struct caif_dev_common cfdev;
	struct net_device *ndev;
	struct platform_device *pdev;
	struct sk_buff_head qhead;
	struct cfhsi_drv drv;
	struct cfhsi_dev *dev;
	int tx_state;
	struct cfhsi_rx_state rx_state;
	unsigned long inactivity_timeout;
	int rx_len;
	u8 *rx_ptr;
	u8 *tx_buf;
	u8 *rx_buf;
	u8 *rx_flip_buf;
	spinlock_t lock;
	int flow_off_sent;
	u32 q_low_mark;
	u32 q_high_mark;
	struct list_head list;
	struct work_struct wake_up_work;
	struct work_struct wake_down_work;
	struct work_struct out_of_sync_work;
	struct workqueue_struct *wq;
	wait_queue_head_t wake_up_wait;
	wait_queue_head_t wake_down_wait;
	wait_queue_head_t flush_fifo_wait;
	struct timer_list timer;
	struct timer_list rx_slowpath_timer;
	unsigned long bits;
#ifdef CONFIG_WAKELOCK
	struct wake_lock link_wakelock;
	struct timer_list sys_wakelock_timer;
	unsigned long last_rx_jiffies;
#endif
};

extern struct platform_driver cfhsi_driver;

#ifdef CONFIG_WAKELOCK
static inline void cfhsi_wakelock_lock(struct cfhsi *cfhsi)
{
	/* Hold link wakelock if not held already */
	if (!test_and_set_bit(CFHSI_WAKELOCK_HELD,
			&cfhsi->bits)) {
		if (!del_timer_sync(&cfhsi->sys_wakelock_timer))
			wake_lock(&cfhsi->link_wakelock);
	}
}

static inline void cfhsi_wakelock_unlock(struct cfhsi *cfhsi)
{
	long tmo = CFHSI_RX_GRACE_PERIOD - (jiffies - cfhsi->last_rx_jiffies);

	/* Release link wakelock */
	if (test_and_clear_bit(CFHSI_WAKELOCK_HELD,
			&cfhsi->bits)) {
		if (tmo > 0 && tmo <= CFHSI_RX_GRACE_PERIOD)
			mod_timer(&cfhsi->sys_wakelock_timer, jiffies + tmo);
		else
			wake_unlock(&cfhsi->link_wakelock);
	}
}

static inline void cfhsi_notify_rx(struct cfhsi *cfhsi)
{
	cfhsi->last_rx_jiffies = jiffies;
}

static void cfhsi_sys_wakelock_tout(unsigned long arg)
{
	struct cfhsi *cfhsi = (struct cfhsi *)arg;

	dev_dbg(&cfhsi->ndev->dev, "%s.\n",
		__func__);

	wake_unlock(&cfhsi->link_wakelock);
}

static inline void cfhsi_wakelock_init(struct cfhsi *cfhsi)
{
	/* Init wakelock which will hold system awake when link is active. */
	wake_lock_init(&cfhsi->link_wakelock, WAKE_LOCK_SUSPEND, "caif_hsi");

	/* Init system timer which holds system in awake state when incoming
	 * CAIF frame is processed by upper layer */
	init_timer(&cfhsi->sys_wakelock_timer);
	cfhsi->sys_wakelock_timer.data = (unsigned long)cfhsi;
	cfhsi->sys_wakelock_timer.function = cfhsi_sys_wakelock_tout;
}

static inline void cfhsi_wakelock_deinit(struct cfhsi *cfhsi)
{
	/* Destroy wakelock. Will also release if it is held. */
	if (del_timer_sync(&cfhsi->sys_wakelock_timer) ||
			test_bit(CFHSI_WAKELOCK_HELD, &cfhsi->bits))
		wake_unlock(&cfhsi->link_wakelock);
	wake_lock_destroy(&cfhsi->link_wakelock);
}
#else /* CONFIG_WAKELOCK */
static inline void cfhsi_wakelock_init(struct cfhsi *cfhsi)
{
}

static inline void cfhsi_wakelock_deinit(struct cfhsi *cfhsi)
{
}

static inline void cfhsi_wakelock_lock(struct cfhsi *cfhsi)
{
}

static inline void cfhsi_wakelock_unlock(struct cfhsi *cfhsi)
{
}

static inline void cfhsi_notify_rx(struct cfhsi *cfhsi)
{
}

#endif /* CONFIG_WAKELOCK */

#endif		/* CAIF_HSI_H_ */
