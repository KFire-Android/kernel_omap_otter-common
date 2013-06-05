/*
 * hdcp_top.c
 *
 * HDCP interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Sujeet Baranwal <s-obaranwal@ti.com>
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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/firmware.h>
#include "hdcp.h"

struct hdcp_data hdcp;
struct hdcp_sha_in sha_input;

struct hdcp_worker_data {
	struct delayed_work dwork;
	atomic_t state;
};

static void hdcp_work_queue(struct work_struct *work);

static DECLARE_WAIT_QUEUE_HEAD(hdcp_up_wait_queue);
static DECLARE_WAIT_QUEUE_HEAD(hdcp_down_wait_queue);

/*------------------------------------------------------------------------------
 * OMAP4 HDCP Support: Start
 *------------------------------------------------------------------------------
 */

#ifdef CONFIG_OMAP4_HDCP_SUPPORT
/* State machine / workqueue */
static void hdcp_wq_disable(void);
static void omap4_hdcp_wq_start_authentication(void);
static void hdcp_wq_check_r0(void);
static void hdcp_wq_step2_authentication(void);
static void hdcp_wq_authentication_failure(void);
static void omap4_hdcp_work_queue(struct work_struct *work);
static struct delayed_work *hdcp_submit_work(int event, int delay);
static void hdcp_cancel_work(struct delayed_work **work);

/* Callbacks */
static void omap4_hdcp_start_frame_cb(void);
static void omap4_hdcp_irq_cb(int hpd_low);

struct completion hdcp_comp;

static int hdcp_wait_re_entrance;

static void hdcp_wq_disable(void)
{
        printk(KERN_INFO "HDCP: disabled\n");

        hdcp_cancel_work(&hdcp.pending_wq_event);
        hdcp_lib_disable();
        hdcp.pending_disable = 0;
}

static void omap4_hdcp_wq_start_authentication(void)
{
	struct hdmi_ip_data *ip_data;
        int status = HDCP_OK;

        hdcp.hdcp_state = HDCP_AUTHENTICATION_START;

        printk(KERN_INFO "HDCP: authentication start\n");

	ip_data = get_hdmi_ip_data();

	if (!ti_hdmi_4xxx_check_rxdet_line(ip_data)) {
		hdcp.hdmi_state = HDMI_POWERED_OFF;
		hdcp.retry_cnt = HDCP_INFINITE_REAUTH;
		hdcp_wq_authentication_failure();
	} else {
		hdcp.hdmi_state = HDMI_STARTED;
	}

        /* Step 1 part 1 (until R0 calc delay) */
        status = hdcp_lib_step1_start();

        if (status == -HDCP_AKSV_ERROR) {
                hdcp_wq_authentication_failure();
        } else if (status == -HDCP_CANCELLED_AUTH) {
                HDCP_DBG("Authentication step 1 cancelled.");
                return;
        } else if (status != HDCP_OK) {
                hdcp_wq_authentication_failure();
        } else {
                hdcp.hdcp_state = HDCP_WAIT_R0_DELAY;
                hdcp.auth_state = HDCP_STATE_AUTH_1ST_STEP;
                hdcp.pending_wq_event = hdcp_submit_work(HDCP_R0_EXP_EVENT,
                                                         HDCP_R0_DELAY);
        }
}

static void hdcp_wq_check_r0(void)
{
        int status = hdcp_lib_step1_r0_check();

        if (status == -HDCP_CANCELLED_AUTH) {
                HDCP_DBG("Authentication step 1/R0 cancelled.");
                return;
        } else if (status < 0)
                hdcp_wq_authentication_failure();
        else {
                if (hdcp_lib_check_repeater_bit_in_tx()) {
                        /* Repeater */
                        printk(KERN_INFO "HDCP: authentication step 1 "
                                         "successful - Repeater\n");

                        hdcp.hdcp_state = HDCP_WAIT_KSV_LIST;
                        hdcp.auth_state = HDCP_STATE_AUTH_2ND_STEP;
                } else {
                        /* Receiver */
                        printk(KERN_INFO "HDCP: authentication step 1 "
                                         "successful - Receiver\n");

                        hdcp.hdcp_state = HDCP_LINK_INTEGRITY_CHECK;
                        hdcp.auth_state = HDCP_STATE_AUTH_3RD_STEP;

                        /* Restore retry counter */
                        if (hdcp.en_ctrl->nb_retry == 0)
                                hdcp.retry_cnt = HDCP_INFINITE_REAUTH;
                        else
                                hdcp.retry_cnt = hdcp.en_ctrl->nb_retry;
                }
        }
}

static void hdcp_wq_step2_authentication(void)
{
        int status = HDCP_OK;

        /* KSV list timeout is running and should be canceled */
        hdcp_cancel_work(&hdcp.pending_wq_event);

        status = hdcp_lib_step2();

        if (status == -HDCP_CANCELLED_AUTH) {
                HDCP_DBG("Authentication step 2 cancelled.");
                return;
        } else if (status < 0)
                hdcp_wq_authentication_failure();
        else {
                printk(KERN_INFO "HDCP: (Repeater) authentication step 2 "
                                 "successful\n");

                hdcp.hdcp_state = HDCP_LINK_INTEGRITY_CHECK;
                hdcp.auth_state = HDCP_STATE_AUTH_3RD_STEP;

                /* Restore retry counter */
                if (hdcp.en_ctrl->nb_retry == 0)
                        hdcp.retry_cnt = HDCP_INFINITE_REAUTH;
                else
                        hdcp.retry_cnt = hdcp.en_ctrl->nb_retry;
        }
}

static void hdcp_wq_authentication_failure(void)
{
        if (hdcp.hdmi_state == HDMI_STOPPED) {
                hdcp.auth_state = HDCP_STATE_AUTH_FAILURE;
                return;
        }

        hdcp_lib_auto_ri_check(false);
        hdcp_lib_auto_bcaps_rdy_check(false);
        hdcp_lib_set_av_mute(AV_MUTE_SET);
        hdcp_lib_set_encryption(HDCP_ENC_OFF);

        hdcp_wq_disable();

        if (hdcp.retry_cnt && (hdcp.hdmi_state != HDMI_STOPPED)) {
                if (hdcp.retry_cnt < HDCP_INFINITE_REAUTH) {
                        hdcp.retry_cnt--;
                        printk(KERN_INFO "HDCP: authentication failed - "
                                         "retrying, attempts=%d\n",
                                                        hdcp.retry_cnt);
                } else
                        printk(KERN_INFO "HDCP: authentication failed - "
                                         "retrying\n");

                hdcp.hdcp_state = HDCP_AUTHENTICATION_START;
                hdcp.auth_state = HDCP_STATE_AUTH_FAIL_RESTARTING;

		if (hdcp.hdmi_state != HDMI_POWERED_OFF)
	                hdcp.pending_wq_event = hdcp_submit_work(
							HDCP_AUTH_REATT_EVENT,
							HDCP_REAUTH_DELAY);
		else
			hdcp.pending_wq_event = hdcp_submit_work(
							HDCP_AUTH_REATT_EVENT,
							HDCP_POWEROFF_DELAY);
        } else {
                printk(KERN_INFO "HDCP: authentication failed - "
                                 "HDCP disabled\n");
                hdcp.hdcp_state = HDCP_ENABLE_PENDING;
                hdcp.auth_state = HDCP_STATE_AUTH_FAILURE;
        }

}

static void omap4_hdcp_work_queue(struct work_struct *work)
{
        struct hdcp_worker_data *hdcp_w =
                container_of(work, struct hdcp_worker_data, dwork.work);
        int event = atomic_read(&hdcp_w->state);

        mutex_lock(&hdcp.lock);

        hdmi_runtime_get();

        HDCP_DBG("hdcp_work_queue() - START - %u hdmi=%d hdcp=%d auth=%d evt= %x %d"
            " hdcp_ctrl=%02x",
                jiffies_to_msecs(jiffies),
                hdcp.hdmi_state,
                hdcp.hdcp_state,
                hdcp.auth_state,
                (event & 0xFF00) >> 8,
                event & 0xFF,
                RD_REG_32(hdcp.hdmi_wp_base_addr + HDMI_IP_CORE_SYSTEM,
                          HDMI_IP_CORE_SYSTEM__HDCP_CTRL));

        /* Clear pending_wq_event
         * In case a delayed work is scheduled from the state machine
         * "pending_wq_event" is used to memorize pointer on the event to be
         * able to cancel any pending work in case HDCP is disabled
         */
        if (event & HDCP_WORKQUEUE_SRC)
                hdcp.pending_wq_event = 0;

        /* First handle HDMI state */
        if (event == HDCP_START_FRAME_EVENT) {
                hdcp.pending_start = 0;
                hdcp.hdmi_state = HDMI_STARTED;
        }
        /**********************/
        /* HDCP state machine */
        /**********************/
        switch (hdcp.hdcp_state) {

        /* State */
        /*********/
        case HDCP_DISABLED:
                /* HDCP enable control or re-authentication event */
                if (event == HDCP_ENABLE_CTL) {
                        if (hdcp.en_ctrl->nb_retry == 0)
                                hdcp.retry_cnt = HDCP_INFINITE_REAUTH;
                        else
                                hdcp.retry_cnt = hdcp.en_ctrl->nb_retry;

                        if (hdcp.hdmi_state == HDMI_STARTED)
                                omap4_hdcp_wq_start_authentication();
                        else
                                hdcp.hdcp_state = HDCP_ENABLE_PENDING;
                }

                break;

        /* State */
        /*********/
        case HDCP_ENABLE_PENDING:
                /* HDMI start frame event */
                if (event == HDCP_START_FRAME_EVENT)
                        omap4_hdcp_wq_start_authentication();

                break;

        /* State */
        /*********/
        case HDCP_AUTHENTICATION_START:
                /* Re-authentication */
                if (event == HDCP_AUTH_REATT_EVENT)
                        omap4_hdcp_wq_start_authentication();

                break;

        /* State */
        /*********/
        case HDCP_WAIT_R0_DELAY:
                /* R0 timer elapsed */
                if (event == HDCP_R0_EXP_EVENT)
                        hdcp_wq_check_r0();

                break;

        /* State */
        /*********/
        case HDCP_WAIT_KSV_LIST:
                /* Ri failure */
                if (event == HDCP_RI_FAIL_EVENT) {
                        printk(KERN_INFO "HDCP: Ri check failure\n");

                        hdcp_wq_authentication_failure();
                }
                /* KSV list ready event */
                else if (event == HDCP_KSV_LIST_RDY_EVENT)
                        hdcp_wq_step2_authentication();
                /* Timeout */
                else if (event == HDCP_KSV_TIMEOUT_EVENT) {
                        printk(KERN_INFO "HDCP: BCAPS polling timeout\n");
                        hdcp_wq_authentication_failure();
                }
                break;

        /* State */
        /*********/
        case HDCP_LINK_INTEGRITY_CHECK:
                /* Ri failure */
                if (event == HDCP_RI_FAIL_EVENT) {
                        printk(KERN_INFO "HDCP: Ri check failure\n");
                        hdcp_wq_authentication_failure();
                }
                break;

        default:
                printk(KERN_WARNING "HDCP: error - unknow HDCP state\n");
                break;
        }

        kfree(hdcp_w);
        hdcp_w = 0;
        if (event == HDCP_START_FRAME_EVENT)
                hdcp.pending_start = 0;
        if (event == HDCP_KSV_LIST_RDY_EVENT ||
            event == HDCP_R0_EXP_EVENT) {
                hdcp.pending_wq_event = 0;
        }

        HDCP_DBG("hdcp_work_queue() - END - %u hdmi=%d hdcp=%d auth=%d evt=%x %d ",
                jiffies_to_msecs(jiffies),
                hdcp.hdmi_state,
                hdcp.hdcp_state,
                hdcp.auth_state,
                (event & 0xFF00) >> 8,
                event & 0xFF);

        hdmi_runtime_put();
        mutex_unlock(&hdcp.lock);
}

static struct delayed_work *hdcp_submit_work(int event, int delay)
{
        struct hdcp_worker_data *work;

        work = kmalloc(sizeof(struct hdcp_worker_data), GFP_ATOMIC);

        if (work) {
                INIT_DELAYED_WORK(&work->dwork, omap4_hdcp_work_queue);
                atomic_set(&work->state, event);
                queue_delayed_work(hdcp.workqueue,
                                   &work->dwork,
                                   msecs_to_jiffies(delay));
        } else {
                printk(KERN_WARNING "HDCP: Cannot allocate memory to "
                                    "create work\n");
                return 0;
        }

        return &work->dwork;
}

static void hdcp_cancel_work(struct delayed_work **work)
{
        int ret = 0;

        if (*work) {
                ret = cancel_delayed_work(*work);
                if (ret != 1) {
                        ret = cancel_work_sync(&((*work)->work));
                        printk(KERN_INFO "Canceling work failed - "
                                         "cancel_work_sync done %d\n", ret);
                }
                kfree(*work);
                *work = 0;
        }
}

int hdcp_user_space_task(int flags)
{
        int ret;

        HDCP_DBG("Wait for user space task %x\n", flags);
        hdcp.hdcp_up_event = flags & 0xFF;
        hdcp.hdcp_down_event = flags & 0xFF;
        wake_up_interruptible(&hdcp_up_wait_queue);
        wait_event_interruptible(hdcp_down_wait_queue,
                                 (hdcp.hdcp_down_event & 0xFF) == 0);
        ret = (hdcp.hdcp_down_event & 0xFF00) >> 8;

        HDCP_DBG("User space task done %x\n", hdcp.hdcp_down_event);
        hdcp.hdcp_down_event = 0;

        return ret;
}

static bool omap4_hdcp_3des_cb(void)
{
        HDCP_DBG("hdcp_3des_cb() %u", jiffies_to_msecs(jiffies));

        if (!hdcp.hdcp_keys_loaded) {
                printk(KERN_ERR "%s: hdcp_keys not loaded = %d",
                       __func__, hdcp.hdcp_keys_loaded);
                return false;
        }

        /* Load 3DES key */
        if (hdcp_3des_load_key(hdcp.en_ctrl->key) != HDCP_OK) {
                printk(KERN_ERR "Error Loading  HDCP keys\n");
                return false;
        }
        return true;
}

static void omap4_hdcp_start_frame_cb(void)
{
        HDCP_DBG("hdcp_start_frame_cb() %u", jiffies_to_msecs(jiffies));

        if (!hdcp.hdcp_keys_loaded) {
                HDCP_DBG("%s: hdcp_keys not loaded = %d",
                    __func__, hdcp.hdcp_keys_loaded);
                return;
        }

        /* Cancel any pending work */
        if (hdcp.pending_start)
                hdcp_cancel_work(&hdcp.pending_start);
        if (hdcp.pending_wq_event)
                hdcp_cancel_work(&hdcp.pending_wq_event);

        hdcp.hpd_low = 0;
        hdcp.pending_disable = 0;
        hdcp.retry_cnt = hdcp.en_ctrl->nb_retry;
        hdcp.pending_start = hdcp_submit_work(HDCP_START_FRAME_EVENT,
                                                        HDCP_ENABLE_DELAY);
}

static void omap4_hdcp_irq_cb(int status)
{
	struct hdmi_ip_data *ip_data;

        HDCP_DBG("hdcp_irq_cb() status=%x", status);

	ip_data = get_hdmi_ip_data();

        if (!hdcp.hdcp_keys_loaded) {
                HDCP_DBG("%s: hdcp_keys not loaded = %d",
                    __func__, hdcp.hdcp_keys_loaded);
                return;
        }

        /* Disable auto Ri/BCAPS immediately */
        if (((status & HDMI_RI_ERR) ||
            (status & HDMI_BCAP) ||
            (status & HDMI_HPD_LOW)) &&
            (hdcp.hdcp_state != HDCP_ENABLE_PENDING)) {
                hdcp_lib_auto_ri_check(false);
                hdcp_lib_auto_bcaps_rdy_check(false);
        }

        /* Work queue execution not required if HDCP is disabled */
        /* TODO: ignore interrupts if they are masked (cannnot access UMASK
         * here so should use global variable
         */
        if ((hdcp.hdcp_state != HDCP_DISABLED) &&
            (hdcp.hdcp_state != HDCP_ENABLE_PENDING)) {
                if (status & HDMI_HPD_LOW) {
                        hdcp_lib_set_encryption(HDCP_ENC_OFF);
                        hdcp_ddc_abort();
                } else if (!ti_hdmi_4xxx_check_rxdet_line(ip_data)) {
			hdcp.hdmi_state = HDMI_POWERED_OFF;
			hdcp.retry_cnt = HDCP_INFINITE_REAUTH;
		}

                if (status & HDMI_RI_ERR) {
                        hdcp_lib_set_av_mute(AV_MUTE_SET);
                        hdcp_lib_set_encryption(HDCP_ENC_OFF);
                        hdcp_submit_work(HDCP_RI_FAIL_EVENT, 0);
                }
                /* RI error takes precedence over BCAP */
                else if (status & HDMI_BCAP)
                        hdcp_submit_work(HDCP_KSV_LIST_RDY_EVENT, 0);
        }

        if (status & HDMI_HPD_LOW) {
                hdcp.pending_disable = 1;       /* Used to exit on-going HDCP
                                                 * work */
                hdcp.hpd_low = 0;               /* Used to cancel HDCP works */
                if (hdcp.pending_start) {
                        pr_err("cancelling work for pending start\n");
                        hdcp_cancel_work(&hdcp.pending_start);
                }
                hdcp_wq_disable();

                /* In case of HDCP_STOP_FRAME_EVENT, HDCP stop
                 * frame callback is blocked and waiting for
                 * HDCP driver to finish accessing the HW
                 * before returning
                 * Reason is to avoid HDMI driver to shutdown
                 * DSS/HDMI power before HDCP work is finished
                 */
                hdcp.hdmi_state = HDMI_STOPPED;
                hdcp.hdcp_state = HDCP_ENABLE_PENDING;
                hdcp.auth_state = HDCP_STATE_DISABLED;
        }
}

static long omap4_hdcp_enable_ctl(void __user *argp)
{
        HDCP_DBG("hdcp_ioctl() - ENABLE %u", jiffies_to_msecs(jiffies));

        if (hdcp.en_ctrl == 0) {
                hdcp.en_ctrl =
                        kmalloc(sizeof(struct hdcp_enable_control),
                                                        GFP_KERNEL);

                if (hdcp.en_ctrl == 0) {
                        printk(KERN_WARNING
                                "HDCP: Cannot allocate memory for HDCP"
                                " enable control struct\n");
                        return -EFAULT;
                }
        }

        if (copy_from_user(hdcp.en_ctrl, argp,
                           sizeof(struct hdcp_enable_control))) {
                printk(KERN_WARNING "HDCP: Error copying from user space "
                                    "- enable ioctl\n");
                return -EFAULT;
        }

        /* Post event to workqueue */
        if (hdcp_submit_work(HDCP_ENABLE_CTL, 0) == 0)
                return -EFAULT;

        return 0;
}

static long omap4_hdcp_disable_ctl(void)
{
        HDCP_DBG("hdcp_ioctl() - DISABLE %u", jiffies_to_msecs(jiffies));

        hdcp_cancel_work(&hdcp.pending_start);
        hdcp_cancel_work(&hdcp.pending_wq_event);

        hdcp.pending_disable = 1;
        /* Post event to workqueue */
        if (hdcp_submit_work(HDCP_DISABLE_CTL, 0) == 0)
                return -EFAULT;

        return 0;
}

static long omap4_hdcp_query_status_ctl(void __user *argp)
{
        uint32_t *status = (uint32_t *)argp;

        HDCP_DBG("hdcp_ioctl() - QUERY %u", jiffies_to_msecs(jiffies));

        *status = hdcp.auth_state;

        return 0;
}

static long omap4_hdcp_wait_event_ctl(void __user *argp)
{
        struct hdcp_wait_control ctrl;

        HDCP_DBG("hdcp_ioctl() - WAIT %u %d", jiffies_to_msecs(jiffies),
                                         hdcp.hdcp_up_event);

        if (copy_from_user(&ctrl, argp,
                           sizeof(struct hdcp_wait_control))) {
                printk(KERN_WARNING "HDCP: Error copying from user space"
                                    " - wait ioctl");
                return -EFAULT;
        }

        if (hdcp_wait_re_entrance == 0) {
                hdcp_wait_re_entrance = 1;
                wait_event_interruptible(hdcp_up_wait_queue,
                                         (hdcp.hdcp_up_event & 0xFF) != 0);

                ctrl.event = hdcp.hdcp_up_event;

                if ((ctrl.event & 0xFF) == HDCP_EVENT_STEP2) {
                        if (copy_to_user(ctrl.data, &sha_input,
                                                sizeof(struct hdcp_sha_in))) {
                                printk(KERN_WARNING "HDCP: Error copying to "
                                                    "user space - wait ioctl");
                                return -EFAULT;
                        }
                }

                hdcp.hdcp_up_event = 0;
                hdcp_wait_re_entrance = 0;
        } else
                ctrl.event = HDCP_EVENT_EXIT;

        /* Store output data to output pointer */
        if (copy_to_user(argp, &ctrl,
                         sizeof(struct hdcp_wait_control))) {
                printk(KERN_WARNING "HDCP: Error copying to user space -"
                                    " wait ioctl");
                return -EFAULT;
        }

        return 0;
}

static long omap4_hdcp_done_ctl(void __user *argp)
{
        uint32_t *status = (uint32_t *)argp;

        HDCP_DBG("hdcp_ioctl() - DONE %u %d", jiffies_to_msecs(jiffies), *status);

        hdcp.hdcp_down_event &= ~(*status & 0xFF);
        hdcp.hdcp_down_event |= *status & 0xFF00;

        wake_up_interruptible(&hdcp_down_wait_queue);

        return 0;
}

static long omap4_hdcp_encrypt_key_ctl(void __user *argp)
{
        struct hdcp_encrypt_control *ctrl;
        uint32_t *out_key;

        HDCP_DBG("hdcp_ioctl() - ENCRYPT KEY %u", jiffies_to_msecs(jiffies));

        mutex_lock(&hdcp.lock);

        if (hdcp.hdcp_state != HDCP_DISABLED) {
                printk(KERN_INFO "HDCP: Cannot encrypt keys while HDCP "
                                   "is enabled\n");
                mutex_unlock(&hdcp.lock);
                return -EFAULT;
        }

        hdcp.hdcp_state = HDCP_KEY_ENCRYPTION_ONGOING;

        /* Encryption happens in ioctl / user context */
        ctrl = kmalloc(sizeof(struct hdcp_encrypt_control),
                       GFP_KERNEL);

        if (ctrl == 0) {
                printk(KERN_WARNING "HDCP: Cannot allocate memory for HDCP"
                                    " encryption control struct\n");
                mutex_unlock(&hdcp.lock);
                return -EFAULT;
        }

        out_key = kmalloc(sizeof(uint32_t) *
                                        DESHDCP_KEY_SIZE, GFP_KERNEL);

        if (out_key == 0) {
                printk(KERN_WARNING "HDCP: Cannot allocate memory for HDCP "
                                    "encryption output key\n");
                kfree(ctrl);
                mutex_unlock(&hdcp.lock);
                return -EFAULT;
        }

        if (copy_from_user(ctrl, argp,
                                sizeof(struct hdcp_encrypt_control))) {
                printk(KERN_WARNING "HDCP: Error copying from user space"
                                    " - encrypt ioctl\n");
                kfree(ctrl);
                kfree(out_key);
                mutex_unlock(&hdcp.lock);
                return -EFAULT;
        }

	hdmi_runtime_get();

        /* Call encrypt function */
        hdcp_3des_encrypt_key(ctrl, out_key);

	hdmi_runtime_put();

        hdcp.hdcp_state = HDCP_DISABLED;
        mutex_unlock(&hdcp.lock);

        /* Store output data to output pointer */
        if (copy_to_user(ctrl->out_key, out_key,
                                sizeof(uint32_t)*DESHDCP_KEY_SIZE)) {
                printk(KERN_WARNING "HDCP: Error copying to user space -"
                                    " encrypt ioctl\n");
                kfree(ctrl);
                kfree(out_key);
                return -EFAULT;
        }

        kfree(ctrl);
        kfree(out_key);
        return 0;
}

long omap4_hdcp_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
        void __user *argp = (void __user *)arg;

        switch (cmd) {
        case HDCP_ENABLE:
                return omap4_hdcp_enable_ctl(argp);

        case HDCP_DISABLE:
                return omap4_hdcp_disable_ctl();

        case HDCP_ENCRYPT_KEY:
                return omap4_hdcp_encrypt_key_ctl(argp);

        case HDCP_QUERY_STATUS:
                return omap4_hdcp_query_status_ctl(argp);

        case HDCP_WAIT_EVENT:
                return omap4_hdcp_wait_event_ctl(argp);

        case HDCP_DONE:
                return omap4_hdcp_done_ctl(argp);

        default:
                return -ENOTTY;
        } /* End switch */
}
#else
static long hdcp_query_status_ctl(uint32_t *status)
{
        struct hdmi_ip_data *ip_data;

        HDCP_DBG("hdcp_ioctl() - QUERY %u", jiffies_to_msecs(jiffies));

        ip_data = get_hdmi_ip_data();
        if (!ip_data) {
                HDCP_ERR("null pointer hit\n");
                return -EINVAL;
        }

        *status = ip_data->ops->hdcp_status(ip_data);

        return 0;
}

static long hdcp_auth_wait_event_ctl(struct hdcp_wait_control *ctrl)
{
        HDCP_DBG("hdcp_ioctl() - WAIT %u %d", jiffies_to_msecs(jiffies),
                                         hdcp.hdcp_up_event);

        wait_event_interruptible(hdcp_up_wait_queue,
                                 (hdcp.hdcp_up_event & 0xFF) != 0);

        ctrl->event = hdcp.hdcp_up_event;

        hdcp.hdcp_up_event = 0;

        return 0;
}

static long hdcp_user_space_done_ctl(uint32_t *status)
{
        HDCP_DBG("hdcp_ioctl() - DONE %u %d", jiffies_to_msecs(jiffies),
                *status);

        hdcp.hdcp_down_event &= ~(*status & 0xFF);
        hdcp.hdcp_down_event |= *status & 0xFF00;

        wake_up_interruptible(&hdcp_down_wait_queue);

        return 0;
}

static long hdcp_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
        void __user *argp = (void __user *)arg;
        struct hdcp_wait_control ctrl;
        uint32_t status;

        if (!fd) {
                HDCP_ERR("%s null pointer\n", __func__);
                return -EINVAL;
        }

        HDCP_DBG("%s\n", __func__);

        switch (cmd) {
        case HDCP_WAIT_EVENT:

                if (copy_from_user(&ctrl, argp,
                                sizeof(struct hdcp_wait_control))) {
                        HDCP_ERR("HDCP: Error copying from user space"
                                    " - wait ioctl");
                        return -EFAULT;
                }

                /* Do not allow re-entrance for this ioctl */
                mutex_lock(&hdcp.re_entrant_lock);
                if (hdcp.re_entrance_flag == false) {
                        hdcp.re_entrance_flag = true;
                        mutex_unlock(&hdcp.re_entrant_lock);

                        hdcp_auth_wait_event_ctl(&ctrl);

                        mutex_lock(&hdcp.re_entrant_lock);
                        hdcp.re_entrance_flag = false;
                        mutex_unlock(&hdcp.re_entrant_lock);
                } else {
                        mutex_unlock(&hdcp.re_entrant_lock);
                        ctrl.event = HDCP_EVENT_EXIT;
                }

                /* Store output data to output pointer */
                if (copy_to_user(argp, &ctrl,
                         sizeof(struct hdcp_wait_control))) {
                        HDCP_ERR("HDCP: Error copying to user space -"
                                    " wait ioctl");
                        return -EFAULT;
                }
                break;

        case HDCP_QUERY_STATUS:

                hdcp_query_status_ctl(&status);

                /* Store output data to output pointer */
                if (copy_to_user(argp, &status,
                        sizeof(uint32_t))) {
                        HDCP_ERR("HDCP: Error copying to user space -"
                                "query status ioctl");
                        return -EFAULT;
                }

                break;

        case HDCP_DONE:
                if (copy_from_user(&status, argp,
                                sizeof(uint32_t))) {
                        HDCP_ERR("HDCP: Error copying from user space"
                                    " - done ioctl");
                        return -EFAULT;
                }

                return hdcp_user_space_done_ctl(&status);

        default:
                return -ENOTTY;
        } /* End switch */

        return 0;
}

static void hdcp_irq_cb(int status)
{
	struct hdmi_ip_data *ip_data;
	u32 intr = 0;

	ip_data = get_hdmi_ip_data();

	if (ip_data->ops->hdcp_int_handler)
		intr = ip_data->ops->hdcp_int_handler(ip_data);

	if (intr == KSVACCESSINT) {
		__cancel_delayed_work(&hdcp.hdcp_work->dwork);
		atomic_set(&hdcp.hdcp_work->state, HDCP_STATE_STEP2);
		queue_delayed_work(hdcp.workqueue, &hdcp.hdcp_work->dwork, 0);
	}

	return;
}

static bool hdcp_3des_cb(void)
{
	HDCP_DBG("hdcp_3des_cb() %u\n", jiffies_to_msecs(jiffies));

	if (!hdcp.hdcp_keys_loaded) {
		HDCP_ERR("%s: hdcp_keys not loaded = %d\n",
				__func__, hdcp.hdcp_keys_loaded);
		return false;
	}

	/* Load 3DES key */
	if (hdcp_3des_load_key(hdcp.en_ctrl->key) != HDCP_OK) {
		HDCP_ERR("Error Loading  HDCP keys\n");
		return false;
	}

	return true;
}

static void hdcp_start_frame_cb(void)
{
	HDCP_DBG("hdcp_start_frame_cb() %ums\n", jiffies_to_msecs(jiffies));

	if (!hdcp.hdcp_keys_loaded) {
		HDCP_DBG("%s: hdcp_keys not loaded = %d\n",
		    __func__, hdcp.hdcp_keys_loaded);
		return;
	}

	/* Cancel any previous work submitted */
	__cancel_delayed_work(&hdcp.hdcp_work->dwork);
	atomic_set(&hdcp.hdcp_work->state, HDCP_STATE_STEP1);
	/* HDCP enable after 7 Vsync delay */
	queue_delayed_work(hdcp.workqueue, &hdcp.hdcp_work->dwork,
				msecs_to_jiffies(300));

}
#endif

/*------------------------------------------------------------------------------
* OMAP4 HDCP Support: End
 *------------------------------------------------------------------------------
 */

static int hdcp_step2_authenticate_repeater(int flags)
{
	int ret;

	HDCP_DBG("Wait for user space task %x\n", flags);
	hdcp.hdcp_up_event = flags & 0xFF;
	hdcp.hdcp_down_event = flags & 0xFF;
	wake_up_interruptible(&hdcp_up_wait_queue);
	wait_event_interruptible(hdcp_down_wait_queue,
				 (hdcp.hdcp_down_event & 0xFF) == 0);
	ret = (hdcp.hdcp_down_event & 0xFF00) >> 8;

	HDCP_DBG("User space task done %x\n", hdcp.hdcp_down_event);
	hdcp.hdcp_down_event = 0;

	return ret;
}

static int hdcp_wq_start_authentication(void)
{
	int status = 0;
	struct hdmi_ip_data *ip_data;

	HDCP_DBG("hdcp_wq_start_authentication %ums\n",
		jiffies_to_msecs(jiffies));

	if (hdmi_runtime_get()) {
		HDCP_ERR("%s Error enabling clocks\n", __func__);
		return -EINVAL;
	}

	ip_data = get_hdmi_ip_data();
	if (ip_data->ops->hdcp_enable)
		ip_data->ops->hdcp_enable(ip_data);
	else
		status = -EINVAL;

	hdmi_runtime_put();
	return status;
}

static void hdcp_work_queue(struct work_struct *work)
{
	struct hdcp_worker_data *d = container_of(work, typeof(*d), dwork.work);
	int state = atomic_read(&d->state);
	int ret = 0;

	HDCP_DBG("hdcp_work_queue() start\n");
	switch (state) {
	case HDCP_STATE_STEP1:
		ret = hdcp_wq_start_authentication();
	break;
	case HDCP_STATE_STEP2:
		ret = hdcp_step2_authenticate_repeater(HDCP_EVENT_STEP2);
	break;
	}

	if (ret)
		HDCP_ERR("authentication failed");

	HDCP_DBG("hdcp_work_queue() - END\n");
}

int hdcp_3des_load_key(uint32_t *deshdcp_encrypted_key)
{
	int counter = 0, status = HDCP_OK;

	if (!deshdcp_encrypted_key) {
		HDCP_ERR("HDCP keys NULL, failed to load keys\n");
		return HDCP_3DES_ERROR;
	}

	HDCP_DBG("Loading HDCP keys...\n");

	/* Set decryption mode in DES control register */
	WR_FIELD_32(hdcp.deshdcp_base_addr,
		DESHDCP__DHDCP_CTRL,
		DESHDCP__DHDCP_CTRL__DIRECTION_POS_F,
		DESHDCP__DHDCP_CTRL__DIRECTION_POS_L,
		0x0);

	/* Write encrypted data */
	while (counter < DESHDCP_KEY_SIZE) {
		/* Fill Data registers */
		WR_REG_32(hdcp.deshdcp_base_addr, DESHDCP__DHDCP_DATA_L,
			deshdcp_encrypted_key[counter]);
		WR_REG_32(hdcp.deshdcp_base_addr, DESHDCP__DHDCP_DATA_H,
			deshdcp_encrypted_key[counter + 1]);

		/* Wait for output bit at '1' */
		while (RD_FIELD_32(hdcp.deshdcp_base_addr,
			DESHDCP__DHDCP_CTRL,
			DESHDCP__DHDCP_CTRL__OUTPUT_READY_POS_F,
			DESHDCP__DHDCP_CTRL__OUTPUT_READY_POS_L) != 0x1)
			;

		/* Dummy read (indeed data are transfered directly into
		 * key memory)
		 */
		if (RD_REG_32(hdcp.deshdcp_base_addr, DESHDCP__DHDCP_DATA_L)
				!= 0x0) {
			status = -HDCP_3DES_ERROR;
			HDCP_ERR("DESHDCP dummy read error\n");
		}
		if (RD_REG_32(hdcp.deshdcp_base_addr, DESHDCP__DHDCP_DATA_H) !=
			0x0) {
			status = -HDCP_3DES_ERROR;
			HDCP_ERR("DESHDCP dummy read error\n");
		}

		counter += 2;
	}

	if (status == HDCP_OK)
		hdcp.hdcp_keys_loaded = true;

	return status;
}

static int hdcp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int status;

	HDCP_DBG("hdcp_mmap() %lx %lx %lx\n", vma->vm_start, vma->vm_pgoff,
					 vma->vm_end - vma->vm_start);

	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	status = remap_pfn_range(vma, vma->vm_start,
				 HDMI_CORE >> PAGE_SHIFT,
				 vma->vm_end - vma->vm_start,
				 vma->vm_page_prot);
	if (status) {
		HDCP_DBG("mmap error %d\n", status);
		return -EAGAIN;
	}

	HDCP_DBG("mmap succesful\n");
	return 0;
}

static void hdcp_load_keys_cb(const struct firmware *fw, void *context)
{
	struct hdcp_enable_control *en_ctrl;

	HDCP_DBG("%s\n", __func__);

	if (!fw) {
		HDCP_ERR("failed to load keys\n");
		return;
	}

	if (fw->size != sizeof(en_ctrl->key)) {
		HDCP_ERR("encrypted key file wrong size %d\n", fw->size);
		return;
	}

	en_ctrl = kzalloc(sizeof(*en_ctrl), GFP_KERNEL);
	if (!en_ctrl) {
		HDCP_ERR("can't allocated space for keys\n");
		return;
	}

	memcpy(en_ctrl->key, fw->data, sizeof(en_ctrl->key));
	en_ctrl->nb_retry = 20;

	hdcp.en_ctrl = en_ctrl;
        hdcp.retry_cnt = hdcp.en_ctrl->nb_retry;
        hdcp.hdcp_state = HDCP_ENABLE_PENDING;
	hdcp.hdcp_keys_loaded = true;
	HDCP_INFO("loaded keys\n");
}

static int hdcp_load_keys(void)
{
	int ret;

	HDCP_DBG("%s\n", __func__);

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					"hdcp.keys", hdcp.mdev->this_device,
					GFP_KERNEL, &hdcp, hdcp_load_keys_cb);
	if (ret < 0) {
		HDCP_ERR("request_firmware_nowait failed: %d\n", ret);
		hdcp.hdcp_keys_loaded = false;
	}

	return ret;
}

static const struct file_operations hdcp_fops = {
	.owner = THIS_MODULE,
	.mmap = hdcp_mmap,
#ifdef CONFIG_OMAP4_HDCP_SUPPORT
	.unlocked_ioctl = omap4_hdcp_ioctl,
#else
	.unlocked_ioctl = hdcp_ioctl,
#endif
};

static int __init hdcp_init(void)
{
	HDCP_DBG("hdcp_init() %ums\n", jiffies_to_msecs(jiffies));

	if (cpu_is_omap44xx()) {
	        /* Map HDMI WP address */
	        hdcp.hdmi_wp_base_addr = ioremap(HDMI_WP, 0x1000);
	        if (!hdcp.hdmi_wp_base_addr) {
	                printk(KERN_ERR "HDCP: HDMI WP IOremap error\n");
	                return -EFAULT;
	        }
	} else {
		hdcp.hdmi_wp_base_addr = NULL;
	}

	hdcp.hdcp_work = kzalloc(sizeof(struct hdcp_worker_data), GFP_KERNEL);
	if (!hdcp.hdcp_work) {
		HDCP_ERR("Could not allocate HDCP worker  structure\n");
		goto err_alloc_work;
	}

	hdcp.mdev = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!hdcp.mdev) {
		HDCP_ERR("Could not allocate misc device memory\n");
		goto err_alloc_mdev;
	}

	/* Map DESHDCP in kernel address space */
	hdcp.deshdcp_base_addr = ioremap(DSS_SS_FROM_L3__DESHDCP, 0x34);

	if (!hdcp.deshdcp_base_addr) {
		HDCP_ERR("DESHDCP IOremap error\n");
		goto err_map_deshdcp;
	}

	mutex_init(&hdcp.lock);

	hdcp.mdev->minor = MISC_DYNAMIC_MINOR;
	hdcp.mdev->name = "hdcp";
	hdcp.mdev->mode = 0666;
	hdcp.mdev->fops = &hdcp_fops;

	if (misc_register(hdcp.mdev)) {
		HDCP_ERR("Could not add character driver\n");
		goto err_misc_register;
	}

	mutex_lock(&hdcp.lock);

        /* Variable init */
        hdcp.en_ctrl  = 0;
        hdcp.hdcp_state = HDCP_DISABLED;
        hdcp.pending_start = 0;
        hdcp.pending_wq_event = 0;
        hdcp.retry_cnt = 0;
        hdcp.auth_state = HDCP_STATE_DISABLED;
        hdcp.pending_disable = 0;
        hdcp.hdcp_up_event = 0;
        hdcp.hdcp_down_event = 0;
        hdcp.hpd_low = 0;

        spin_lock_init(&hdcp.spinlock);

#ifdef CONFIG_OMAP4_HDCP_SUPPORT
        hdcp_wait_re_entrance = 0;
        init_completion(&hdcp_comp);
#endif

	hdcp.workqueue = create_singlethread_workqueue("hdcp");
	if (hdcp.workqueue == NULL) {
		HDCP_ERR("Could not create HDCP workqueue\n");
		goto err_add_driver;
	}

	INIT_DELAYED_WORK(&hdcp.hdcp_work->dwork, hdcp_work_queue);

	mutex_init(&hdcp.re_entrant_lock);

	if (hdmi_runtime_get()) {
		HDCP_ERR("%s Error enabling clocks\n", __func__);
		goto err_runtime;
	}

	/* Register HDCP callbacks to HDMI library */
#ifdef CONFIG_OMAP4_HDCP_SUPPORT
		omapdss_hdmi_register_hdcp_callbacks(&omap4_hdcp_start_frame_cb,
				 &omap4_hdcp_3des_cb, &omap4_hdcp_irq_cb);
#else
		omapdss_hdmi_register_hdcp_callbacks(&hdcp_start_frame_cb,
				 &hdcp_3des_cb, &hdcp_irq_cb);
#endif

	hdmi_runtime_put();

        mutex_unlock(&hdcp.lock);

	hdcp_load_keys();

	return 0;

err_runtime:
	mutex_destroy(&hdcp.re_entrant_lock);

	destroy_workqueue(hdcp.workqueue);

err_add_driver:
	misc_deregister(hdcp.mdev);

	mutex_unlock(&hdcp.lock);

err_misc_register:
	mutex_destroy(&hdcp.lock);

	iounmap(hdcp.deshdcp_base_addr);

err_map_deshdcp:
	kfree(hdcp.mdev);

err_alloc_mdev:
	kfree(hdcp.hdcp_work);

err_alloc_work:
	if (cpu_is_omap44xx())
		iounmap(hdcp.hdmi_wp_base_addr);

	return -EFAULT;
}

static void __exit hdcp_exit(void)
{
	HDCP_DBG("hdcp_exit() %ums\n", jiffies_to_msecs(jiffies));

	mutex_lock(&hdcp.lock);

	kfree(hdcp.en_ctrl);

	if (hdmi_runtime_get()) {
		HDCP_ERR("%s Error enabling clocks\n", __func__);
		goto err_handling;
	}

	/* Un-register HDCP callbacks to HDMI library */
	omapdss_hdmi_register_hdcp_callbacks(NULL, NULL, NULL);

	hdmi_runtime_put();

err_handling:
	misc_deregister(hdcp.mdev);
	kfree(hdcp.mdev);

	iounmap(hdcp.deshdcp_base_addr);

	if (cpu_is_omap44xx())
		iounmap(hdcp.hdmi_wp_base_addr);

	kfree(hdcp.hdcp_work);
	destroy_workqueue(hdcp.workqueue);

	mutex_unlock(&hdcp.lock);

	mutex_destroy(&hdcp.lock);

	mutex_destroy(&hdcp.re_entrant_lock);
}

module_init(hdcp_init);
module_exit(hdcp_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OMAP HDCP kernel module");
MODULE_AUTHOR("Sujeet Baranwal");
