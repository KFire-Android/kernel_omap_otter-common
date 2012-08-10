/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland / sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/tty.h>
#include <linux/file.h>
#include <linux/if_arp.h>
#include <net/caif/caif_device.h>
#include <net/caif/cfcnfg.h>
#include <net/caif/caif_serial.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sjur Brendeland<sjur.brandeland@stericsson.com>");
MODULE_DESCRIPTION("CAIF serial device TTY line discipline");
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_CAIF);

#define SEND_QUEUE_LOW 10
#define SEND_QUEUE_HIGH 100
#define CAIF_SENDING		1 /* Bit 1 = 0x02*/
#define CAIF_FLOW_OFF_SENT	4 /* Bit 4 = 0x10 */
#define MAX_WRITE_CHUNK	     4096
#define ON 1
#define OFF 0
#define CAIF_MAX_MTU 4096

/*This list is protected by the rtnl lock. */
static LIST_HEAD(ser_list);

static int ser_loop;
module_param(ser_loop, bool, S_IRUGO);
MODULE_PARM_DESC(ser_loop, "Run in simulated loopback mode.");

static int ser_use_stx = 1;
module_param(ser_use_stx, bool, S_IRUGO);
MODULE_PARM_DESC(ser_use_stx, "STX enabled or not.");

static int ser_use_fcs = 1;

#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
#define MS_TO_JIFFIES(x)	((x)*HZ/1000)
#define CWR_TOUT_MS		1000
#define CWR_TOUT		(MS_TO_JIFFIES(CWR_TOUT_MS))

#define POWER_BIT		5

#define CWR_EDGE_ACTIVE_BIT	5
#define CWR_EDGE_INACTIVE_BIT	6
#define TX_ACTIVE_BIT		7
#define RX_ACTIVE_BIT		8
#define PM_STATE_ON_BIT		9
#define CWR_CHECK_BIT		10

#define IS_CWR_EDGE_ACTIVE	(test_bit(CWR_EDGE_ACTIVE_BIT, &ser->state))
#define IS_CWR_EDGE_INACTIVE	(test_bit(CWR_EDGE_INACTIVE_BIT, &ser->state))
#define IS_TX_ACTIVE		(test_bit(TX_ACTIVE_BIT, &ser->state))
#define IS_RX_ACTIVE		(test_bit(RX_ACTIVE_BIT, &ser->state))

/* Power management states. */
#define PM_STATE_WAIT_FOR_WAKEUP 0
#define PM_STATE_WAIT_FOR_CWR_HI 1
#define PM_STATE_WAIT_FOR_TX_IDLE 2
#define PM_STATE_WAIT_FOR_TX_FLUSH 3
#define PM_STATE_WAIT_FOR_RX_IDLE 4
#define PM_STATE_WAIT_FOR_CWR_LO 5

static unsigned long power_state;

static int inactivity = 1000;
module_param(inactivity, int, S_IRUGO);
MODULE_PARM_DESC(devices, "Number of milliseconds for the inactivity timer.");
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/

module_param(ser_use_fcs, bool, S_IRUGO);
MODULE_PARM_DESC(ser_use_fcs, "FCS enabled or not.");

static int ser_write_chunk = MAX_WRITE_CHUNK;
module_param(ser_write_chunk, int, S_IRUGO);

MODULE_PARM_DESC(ser_write_chunk, "Maximum size of data written to UART.");

static struct dentry *debugfsdir;

static int caif_net_open(struct net_device *dev);
static int caif_net_close(struct net_device *dev);

struct ser_device {
	struct caif_dev_common common;
	struct list_head node;
	struct net_device *dev;
	struct sk_buff_head head;
	struct tty_struct *tty;
	bool tx_started;
	unsigned long state;
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	wait_queue_head_t pm_wait;
	struct work_struct pm_work;
	struct workqueue_struct *pm_wq;
	bool terminate;
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_tty_dir;
	struct debugfs_blob_wrapper tx_blob;
	struct debugfs_blob_wrapper rx_blob;
	u8 rx_data[128];
	u8 tx_data[128];
	u8 tty_status;
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	u8 pm_status;
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
#endif /* CONFIG_DEBUG_FS */
};

static void caifdev_setup(struct net_device *dev);
static void ldisc_tx_wakeup(struct tty_struct *tty);
#ifdef CONFIG_DEBUG_FS
static inline void update_tty_status(struct ser_device *ser)
{
	ser->tty_status =
		ser->tty->stopped << 5 |
		ser->tty->hw_stopped << 4 |
		ser->tty->flow_stopped << 3 |
		ser->tty->packet << 2 |
		ser->tty->low_latency << 1 |
		ser->tty->warned;
}
static inline void debugfs_init(struct ser_device *ser, struct tty_struct *tty)
{
	ser->debugfs_tty_dir =
			debugfs_create_dir(tty->name, debugfsdir);
	if (!IS_ERR(ser->debugfs_tty_dir)) {
		debugfs_create_blob("last_tx_msg", S_IRUSR,
				ser->debugfs_tty_dir,
				&ser->tx_blob);

		debugfs_create_blob("last_rx_msg", S_IRUSR,
				ser->debugfs_tty_dir,
				&ser->rx_blob);

		debugfs_create_x32("ser_state", S_IRUSR,
				ser->debugfs_tty_dir,
				(u32 *)&ser->state);

		debugfs_create_x8("tty_status", S_IRUSR,
				ser->debugfs_tty_dir,
				&ser->tty_status);

#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
		debugfs_create_x8("pm_state", S_IRUSR,
				ser->debugfs_tty_dir,
				&ser->pm_status);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	}
	ser->tx_blob.data = ser->tx_data;
	ser->tx_blob.size = 0;
	ser->rx_blob.data = ser->rx_data;
	ser->rx_blob.size = 0;
}

static inline void debugfs_deinit(struct ser_device *ser)
{
	debugfs_remove_recursive(ser->debugfs_tty_dir);
}

static inline void debugfs_rx(struct ser_device *ser, const u8 *data, int size)
{
	if (size > sizeof(ser->rx_data))
		size = sizeof(ser->rx_data);
	memcpy(ser->rx_data, data, size);
	ser->rx_blob.data = ser->rx_data;
	ser->rx_blob.size = size;
}

static inline void debugfs_tx(struct ser_device *ser, const u8 *data, int size)
{
	if (size > sizeof(ser->tx_data))
		size = sizeof(ser->tx_data);
	memcpy(ser->tx_data, data, size);
	ser->tx_blob.data = ser->tx_data;
	ser->tx_blob.size = size;
}
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
static inline void update_pm_status(struct ser_device *ser, int status)
{
	ser->pm_status = status;
}
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
#else
static inline void debugfs_init(struct ser_device *ser, struct tty_struct *tty)
{
}

static inline void debugfs_deinit(struct ser_device *ser)
{
}

static inline void update_tty_status(struct ser_device *ser)
{
}

static inline void debugfs_rx(struct ser_device *ser, const u8 *data, int size)
{
}

static inline void debugfs_tx(struct ser_device *ser, const u8 *data, int size)
{
}
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
static inline void update_pm_status(struct ser_device *ser, int status)
{
}
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
#endif /* CONFIG_DEBUG_FS */

#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
static int handle_tx(struct ser_device *ser);


void power_notify(bool is_on)
{
	if (is_on)
		set_bit(POWER_BIT, &power_state);
	else
		clear_bit(POWER_BIT, &power_state);
}

void cwr_notify(struct tty_struct *tty, bool asserted)
{
	struct ser_device *ser = NULL;
	struct list_head *node;
	bool tty_found = false;

	list_for_each(node, &ser_list) {
		ser = list_entry(node, struct ser_device, node);
		/* Find the corresponding device using tty as mapping. */
		if (ser->tty == tty) {
			tty_found = true;
			break;
		}
	}

	BUG_ON(!tty_found);

	if (asserted) {
		if (test_and_set_bit(CWR_CHECK_BIT, &ser->state)) {
			pr_warn("Assume missed CWR edge (set)\n");
			if (test_and_set_bit(CWR_EDGE_INACTIVE_BIT,
							&ser->state))
				pr_warn("Inconsistent CWR state\n");
		}
		if (test_and_set_bit(CWR_EDGE_ACTIVE_BIT, &ser->state))
			pr_warn("Inconsistent CWR state. Race ?\n");
	} else {
		if (!test_and_clear_bit(CWR_CHECK_BIT, &ser->state)) {
			pr_warn("Assume missed CWR edge (clear)\n");
			if (test_and_set_bit(CWR_EDGE_ACTIVE_BIT, &ser->state))
				pr_warn("Inconsistent CWR state\n");
		}
		if (test_and_set_bit(CWR_EDGE_INACTIVE_BIT, &ser->state))
			pr_warn("Inconsistent CWR state. Race ?\n");
	}

	wake_up_interruptible(&ser->pm_wait);
}

static void pm_work(struct work_struct *work)
{
	long ret;
	struct ser_device *ser;

	ser = container_of(work, struct ser_device, pm_work);

	if (pm_ext_init(ser->tty)) {
		printk(KERN_ERR "pm_work: pm_ext_init failed\n");
		return;
	}

 wait_for_wakeup:
	update_pm_status(ser, PM_STATE_WAIT_FOR_WAKEUP);
	ret = wait_event_interruptible(ser->pm_wait,
				       IS_CWR_EDGE_ACTIVE | IS_TX_ACTIVE);

	/* Interrupted by signal */
	if (ret < 0) {
		printk(KERN_ERR "cfser_pm_work: Received signal: %ld\n", ret);
		return;
	}

	/* Check termination condition. */
	if (ser->terminate)
		return;

	/* Wait for resume to finish */
	while (!test_bit(POWER_BIT, &power_state))
		usleep_range(500, 1500);

	/* Prepare for going up (Assert AWR). */
	pm_ext_up(ser->tty);

 wait_for_cwr_high:
	update_pm_status(ser, PM_STATE_WAIT_FOR_CWR_HI);
	/* Wait for CWR assertion. */
	ret = wait_event_interruptible_timeout(ser->pm_wait,
					       IS_CWR_EDGE_ACTIVE,
					       MS_TO_JIFFIES(inactivity));

	/* Interrupted by signal */
	if (ret < 0) {
		printk(KERN_ERR "cfser_pm_work: Received signal: %ld\n", ret);
		return;
	}

	/* Check termination condition. */
	if (ser->terminate)
		return;

	if (!IS_CWR_EDGE_ACTIVE) {
		printk(KERN_INFO "cfser_pm_work: CWR assertion tout\n");
		goto wait_for_cwr_high;
	}
	clear_bit(CWR_EDGE_ACTIVE_BIT, &ser->state);

	/* We can now start transmitting. */
	set_bit(PM_STATE_ON_BIT, &ser->state);
	if (IS_TX_ACTIVE)
		handle_tx(ser);

 wait_for_tx_idle:
	update_pm_status(ser, PM_STATE_WAIT_FOR_TX_IDLE);
	/* Wait until transmission is idle. */
	ret = wait_event_interruptible(ser->pm_wait, !IS_TX_ACTIVE);

	/* Interrupted by signal */
	if (ret < 0) {
		printk(KERN_ERR "cfser_pm_work: Received signal: %ld\n", ret);
		return;
	}

	/* Check termination condition. */
	if (ser->terminate)
		return;

	update_pm_status(ser, PM_STATE_WAIT_FOR_TX_FLUSH);
	/* Block until tty buffer is empty (if available). */
	if (ser->tty->ops->wait_until_sent)
		ser->tty->ops->wait_until_sent(ser->tty, 0);
	else
		WARN_ONCE(1, "TTY does not implement wait_until_sent\n");

 wait_for_rx_idle:
	update_pm_status(ser, PM_STATE_WAIT_FOR_RX_IDLE);
	/* Wait until the reception times out or transmission resumes. */
	clear_bit(RX_ACTIVE_BIT, &ser->state);
	ret = wait_event_interruptible_timeout(ser->pm_wait,
					       IS_TX_ACTIVE || IS_RX_ACTIVE,
					       MS_TO_JIFFIES(inactivity));

	/* Interrupted by signal */
	if (ret < 0) {
		printk(KERN_ERR "cfser_pm_work: Received signal: %ld\n", ret);
		return;
	}

	/* Check termination condition. */
	if (ser->terminate)
		return;

	if (IS_TX_ACTIVE)
		goto wait_for_tx_idle;

	if (IS_RX_ACTIVE)
		goto wait_for_rx_idle;

	/* We can no longer transmit. */
	clear_bit(PM_STATE_ON_BIT, &ser->state);

	/* Prepare for going down (deassert AWR). */
	pm_ext_prep_down(ser->tty);

 wait_for_cwr_low:
	update_pm_status(ser, PM_STATE_WAIT_FOR_CWR_LO);
	ret = wait_event_interruptible_timeout(ser->pm_wait,
					       IS_CWR_EDGE_INACTIVE,
					       MS_TO_JIFFIES(inactivity));
	/* Interrupted by signal */
	if (ret < 0) {
		printk(KERN_ERR "cfser_pm_work: Received signal: %ld\n", ret);
		return;
	}

	/* Check termination condition. */
	if (ser->terminate)
		return;

	if (!IS_CWR_EDGE_INACTIVE) {
		printk(KERN_INFO "cfser_pm_work: CWR de-assertion tout\n");
		goto wait_for_cwr_low;
	}
	clear_bit(CWR_EDGE_INACTIVE_BIT, &ser->state);

	/* Prepare for going down (deassert AWR). */
	pm_ext_down(ser->tty);

	/*
	 * EMARDAN: TODO: Change me !
	 * Delay to make sure that the GPIO toggling time is respected with
	 * regards to consecutive changes of the AWR signal state. The AWR
	 * is not allowed to change earlier than 1/32768 seconds after the
	 * last change.
	 */
	usleep_range(1000, 1500);

	/* Back in OFF mode. */
	goto wait_for_wakeup;

}
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/

static void ldisc_receive(struct tty_struct *tty, const u8 *data,
			char *flags, int count)
{
	struct sk_buff *skb = NULL;
	struct ser_device *ser;
	int ret;
	u8 *p;

	ser = tty->disc_data;

	/*
	 * NOTE: flags may contain information about break or overrun.
	 * This is not yet handled.
	 */
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	if (!test_and_set_bit(RX_ACTIVE_BIT, &ser->state))
		wake_up_interruptible(&ser->pm_wait);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	BUG_ON(ser->dev == NULL);

	/*
	 * Workaround for garbage at start of transmission,
	 * only enable if STX handling is not enabled.
	 */
	if (!ser->common.use_stx && !ser->tx_started) {
		dev_info(&ser->dev->dev,
			"Bytes received before initial transmission -"
			"bytes discarded\n");
		return;
	}

	BUG_ON(ser->dev == NULL);

	/* Get a suitable caif packet and copy in data. */
	skb = netdev_alloc_skb(ser->dev, count+1);
	if (skb == NULL)
		return;
	p = skb_put(skb, count);
	memcpy(p, data, count);

	skb->protocol = htons(ETH_P_CAIF);
	skb_reset_mac_header(skb);
	skb->dev = ser->dev;
	debugfs_rx(ser, data, count);
	/* Push received packet up the stack. */
	ret = netif_rx_ni(skb);
	if (!ret) {
		ser->dev->stats.rx_packets++;
		ser->dev->stats.rx_bytes += count;
	} else
		++ser->dev->stats.rx_dropped;
	update_tty_status(ser);
}

static int handle_tx(struct ser_device *ser)
{
	struct tty_struct *tty;
	struct sk_buff *skb;
	int tty_wr, len, room;

	tty = ser->tty;
	ser->tx_started = true;

	/* Enter critical section */
	if (test_and_set_bit(CAIF_SENDING, &ser->state))
		return 0;

	/* skb_peek is safe because handle_tx is called after skb_queue_tail */
	while ((skb = skb_peek(&ser->head)) != NULL) {
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
		if (!test_and_set_bit(TX_ACTIVE_BIT, &ser->state))
			wake_up_interruptible(&ser->pm_wait);

		if (!test_bit(PM_STATE_ON_BIT, &ser->state))
			break;
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/

		/* Make sure you don't write too much */
		len = skb->len;
		room = tty_write_room(tty);
		if (!room)
			break;
		if (room > ser_write_chunk)
			room = ser_write_chunk;
		if (len > room)
			len = room;

		/* Write to tty or loopback */
		if (!ser_loop) {
			tty_wr = tty->ops->write(tty, skb->data, len);
			update_tty_status(ser);
		} else {
			tty_wr = len;
			ldisc_receive(tty, skb->data, NULL, len);
		}
		ser->dev->stats.tx_packets++;
		ser->dev->stats.tx_bytes += tty_wr;

		/* Error on TTY ?! */
		if (tty_wr < 0)
			goto error;
		/* Reduce buffer written, and discard if empty */
		skb_pull(skb, tty_wr);
		if (skb->len == 0) {
			struct sk_buff *tmp = skb_dequeue(&ser->head);
			WARN_ON(tmp != skb);
			if (in_interrupt())
				dev_kfree_skb_irq(skb);
			else
				kfree_skb(skb);
		}
	}
	/* Send flow off if queue is empty */
	if (ser->head.qlen <= SEND_QUEUE_LOW &&
		test_and_clear_bit(CAIF_FLOW_OFF_SENT, &ser->state) &&
		ser->common.flowctrl != NULL)
				ser->common.flowctrl(ser->dev, ON);

#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	/* Report idle if queue is empty */
	if (!ser->head.qlen && test_and_clear_bit(TX_ACTIVE_BIT, &ser->state))
		wake_up_interruptible(&ser->pm_wait);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	clear_bit(CAIF_SENDING, &ser->state);
	return 0;
error:
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	if (test_and_clear_bit(TX_ACTIVE_BIT, &ser->state))
		wake_up_interruptible(&ser->pm_wait);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	clear_bit(CAIF_SENDING, &ser->state);
	return tty_wr;
}

static int caif_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ser_device *ser;

	BUG_ON(dev == NULL);
	ser = netdev_priv(dev);

	/* Send flow off once, on high water mark */
	if (ser->head.qlen > SEND_QUEUE_HIGH &&
		!test_and_set_bit(CAIF_FLOW_OFF_SENT, &ser->state) &&
		ser->common.flowctrl != NULL)

		ser->common.flowctrl(ser->dev, OFF);

	skb_queue_tail(&ser->head, skb);
	return handle_tx(ser);
}


static void ldisc_tx_wakeup(struct tty_struct *tty)
{
	struct ser_device *ser;

	ser = tty->disc_data;
	BUG_ON(ser == NULL);
	WARN_ON(ser->tty != tty);
	handle_tx(ser);
}


static int ldisc_open(struct tty_struct *tty)
{
	struct ser_device *ser;
	struct net_device *dev;
	char name[64];
	int result;

	/* No write no play */
	if (tty->ops->write == NULL)
		return -EOPNOTSUPP;
	if (!capable(CAP_SYS_ADMIN) && !capable(CAP_SYS_TTY_CONFIG))
		return -EPERM;

	sprintf(name, "cf%s", tty->name);
	dev = alloc_netdev(sizeof(*ser), name, caifdev_setup);
	if (!dev)
		return -ENOMEM;

	ser = netdev_priv(dev);
	ser->tty = tty_kref_get(tty);
	ser->dev = dev;
	debugfs_init(ser, tty);
	tty->receive_room = N_TTY_BUF_SIZE;
	tty->disc_data = ser;
	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	init_waitqueue_head(&ser->pm_wait);
	INIT_WORK(&ser->pm_work, pm_work);
	ser->pm_wq = create_singlethread_workqueue(name);
	if (!ser->pm_wq) {
		pr_warn("ldisc_open: failed to create work queue\n");
		free_netdev(dev);
		return -ENODEV;
	}
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	rtnl_lock();
	result = register_netdevice(dev);
	if (result) {
		rtnl_unlock();
		free_netdev(dev);
		return -ENODEV;
	}

	list_add(&ser->node, &ser_list);
	rtnl_unlock();
	netif_stop_queue(dev);
	update_tty_status(ser);

#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	/* EMARDAN: Fixme:  We need the list. */
	printk(KERN_INFO "ldisc_open: PM schedule\n");
	queue_work(ser->pm_wq, &ser->pm_work);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/

	return 0;
}

static void ldisc_close(struct tty_struct *tty)
{
	struct ser_device *ser = tty->disc_data;
	/* Remove may be called inside or outside of rtnl_lock */
	int islocked = rtnl_is_locked();

	if (!islocked)
		rtnl_lock();
	/* device is freed automagically by net-sysfs */
	dev_close(ser->dev);
	unregister_netdevice(ser->dev);
	list_del(&ser->node);
	debugfs_deinit(ser);
	tty_kref_put(ser->tty);
	if (!islocked)
		rtnl_unlock();
}

/* The line discipline structure. */
static struct tty_ldisc_ops caif_ldisc = {
	.owner =	THIS_MODULE,
	.magic =	TTY_LDISC_MAGIC,
	.name =		"n_caif",
	.open =		ldisc_open,
	.close =	ldisc_close,
	.receive_buf =	ldisc_receive,
	.write_wakeup =	ldisc_tx_wakeup
};

static int register_ldisc(void)
{
	int result;

	result = tty_register_ldisc(N_CAIF, &caif_ldisc);
	if (result < 0) {
		pr_err("cannot register CAIF ldisc=%d err=%d\n", N_CAIF,
			result);
		return result;
	}
	return result;
}
static const struct net_device_ops netdev_ops = {
	.ndo_open = caif_net_open,
	.ndo_stop = caif_net_close,
	.ndo_start_xmit = caif_xmit
};

static void caifdev_setup(struct net_device *dev)
{
	struct ser_device *serdev = netdev_priv(dev);

	dev->features = 0;
	dev->netdev_ops = &netdev_ops;
	dev->type = ARPHRD_CAIF;
	dev->flags = IFF_POINTOPOINT | IFF_NOARP;
	dev->mtu = CAIF_MAX_MTU;
	dev->tx_queue_len = 0;
	dev->destructor = free_netdev;
	skb_queue_head_init(&serdev->head);
	serdev->common.link_select = CAIF_LINK_LOW_LATENCY;
	serdev->common.use_frag = true;
	serdev->common.use_stx = ser_use_stx;
	serdev->common.use_fcs = ser_use_fcs;
	serdev->dev = dev;
}


static int caif_net_open(struct net_device *dev)
{
	netif_wake_queue(dev);
	return 0;
}

static int caif_net_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	return 0;
}

static int __init caif_ser_init(void)
{
	int ret;

	ret = register_ldisc();
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
	power_notify(true);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
	debugfsdir = debugfs_create_dir("caif_serial", NULL);
	return ret;
}

static void __exit caif_ser_exit(void)
{
	struct ser_device *ser = NULL;
	struct list_head *node;
	struct list_head *_tmp;

	list_for_each_safe(node, _tmp, &ser_list) {
		ser = list_entry(node, struct ser_device, node);
#ifdef CONFIG_CAIF_USE_DEPRECATED_FUNC
#ifdef CONFIG_CAIF_TTY_PM
		ser->terminate = true;
		wake_up_interruptible(&ser->pm_wait);
		destroy_workqueue(ser->pm_wq);
		pm_ext_deinit(ser->tty);
#endif /* CONFIG_CAIF_TTY_PM */
#endif /*CONFIG_CAIF_USE_DEPRECATED_FUNC*/
		dev_close(ser->dev);
		unregister_netdevice(ser->dev);
		list_del(node);
	}
	tty_unregister_ldisc(N_CAIF);
	debugfs_remove_recursive(debugfsdir);
}

module_init(caif_ser_init);
module_exit(caif_ser_exit);
