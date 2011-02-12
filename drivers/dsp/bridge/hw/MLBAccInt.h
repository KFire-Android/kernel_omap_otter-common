/*
 * MLBAccInt.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _MLB_ACC_INT_H
#define _MLB_ACC_INT_H

/* Mappings of level 1 EASI function numbers to function names */

#define EASIL1_MLBMAILBOX_SYSCONFIG_READ_REGISTER32   (MLB_BASE_EASIL1 + 3)
#define EASIL1_MLBMAILBOX_SYSCONFIG_WRITE_REGISTER32  (MLB_BASE_EASIL1 + 4)
#define EASIL1_MLBMAILBOX_SYSCONFIGS_IDLE_MODE_READ32   (MLB_BASE_EASIL1 + 7)
#define EASIL1_MLBMAILBOX_SYSCONFIGS_IDLE_MODE_WRITE32  (MLB_BASE_EASIL1 + 17)
#define EASIL1_MLBMAILBOX_SYSCONFIG_SOFT_RESET_WRITE32 (MLB_BASE_EASIL1 + 29)
#define EASIL1_MLBMAILBOX_SYSCONFIG_AUTO_IDLE_READ32 \
						(MLB_BASE_EASIL1 + 33)
#define EASIL1_MLBMAILBOX_SYSCONFIG_AUTO_IDLE_WRITE32   (MLB_BASE_EASIL1 + 39)
#define EASIL1_MLBMAILBOX_SYSSTATUS_RESET_DONE_READ32  (MLB_BASE_EASIL1 + 44)
#define EASIL1_MLBMAILBOX_MESSAGE015_READ_REGISTER32 \
						(MLB_BASE_EASIL1 + 50)
#define EASIL1_MLBMAILBOX_MESSAGE015_WRITE_REGISTER32  \
						(MLB_BASE_EASIL1 + 51)
#define EASIL1_MLBMAILBOX_FIFOSTATUS015_READ_REGISTER32  \
						(MLB_BASE_EASIL1 + 56)
#define EASIL1_MLBMAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_READ32 \
						(MLB_BASE_EASIL1 + 57)
#define EASIL1_MLBMAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_READ32  \
						(MLB_BASE_EASIL1 + 60)
#define EASIL1_MLBMAILBOX_IRQSTATUS03_READ_REGISTER32  \
						(MLB_BASE_EASIL1 + 62)
#define EASIL1_MLBMAILBOX_IRQSTATUS03_WRITE_REGISTER32 \
						(MLB_BASE_EASIL1 + 63)
#define EASIL1_MLBMAILBOX_IRQENABLE03_READ_REGISTER32    \
						(MLB_BASE_EASIL1 + 192)
#define EASIL1_MLBMAILBOX_IRQENABLE03_WRITE_REGISTER32   \
						(MLB_BASE_EASIL1 + 193)

/* Register set MAILBOX_MESSAGE___REGSET_0_15 address offset, bank address
 * increment and number of banks */

#define MLB_MAILBOX_MESSAGE_REGSET015_OFFSET    (u32)(0x0040)
#define MLB_MAILBOX_MESSAGE_REGSET015_STEP   (u32)(0x0004)

/* Register offset address definitions relative to register set
 * MAILBOX_MESSAGE___REGSET_0_15 */

#define MLB_MAILBOX_MESSAGE015_OFFSET   (u32)(0x0)

/* Register set MAILBOX_FIFOSTATUS___REGSET_0_15 address offset, bank address
 * increment and number of banks */

#define MLB_MAILBOX_FIFOSTATUS_REGSET015_OFFSET  (u32)(0x0080)
#define MLB_MAILBOX_FIFOSTATUS_REGSET015_STEP   (u32)(0x0004)

/* Register offset address definitions relative to register set
 * MAILBOX_FIFOSTATUS___REGSET_0_15 */

#define MLB_MAILBOX_FIFOSTATUS015_OFFSET    (u32)(0x0)

/* Register set MAILBOX_MSGSTATUS___REGSET_0_15 address offset, bank address
 * increment and number of banks */

#define MLB_MAILBOX_MSGSTATUS_REGSET015_OFFSET  (u32)(0x00c0)
#define MLB_MAILBOX_MSGSTATUS_REGSET015_STEP    (u32)(0x0004)

/* Register offset address definitions relative to register set
 * MAILBOX_MSGSTATUS___REGSET_0_15 */

#define MLB_MAILBOX_MSGSTATUS015_OFFSET    (u32)(0x0)

/* Register set MAILBOX_IRQSTATUS___REGSET_0_3 address offset, bank address
 * increment and number of banks */

#define MLB_MAILBOX_IRQSTATUS_REGSET03_OFFSET        (u32)(0x0100)
#define MLB_MAILBOX_IRQSTATUS_REGSET03_STEP          (u32)(0x0008)

/* Register offset address definitions relative to register set
 * MAILBOX_IRQSTATUS___REGSET_0_3 */

#define MLB_MAILBOX_IRQSTATUS03_OFFSET        (u32)(0x0)

/* Register set MAILBOX_IRQENABLE___REGSET_0_3 address offset, bank address
 * increment and number of banks */

#define MLB_MAILBOX_IRQENABLE_REGSET03_OFFSET     (u32)(0x0104)
#define MLB_MAILBOX_IRQENABLE_REGSET03_STEP     (u32)(0x0008)

/* Register offset address definitions relative to register set
 * MAILBOX_IRQENABLE___REGSET_0_3 */

#define MLB_MAILBOX_IRQENABLE03_OFFSET          (u32)(0x0)

/* Register offset address definitions */

#define MLB_MAILBOX_SYSCONFIG_OFFSET            (u32)(0x10)
#define MLB_MAILBOX_SYSSTATUS_OFFSET            (u32)(0x14)

/* Bitfield mask and offset declarations */

#define MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_MASK        (u32)(0x18)
#define MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_OFFSET      (u32)(3)
#define MLB_MAILBOX_SYSCONFIG_SOFT_RESET_MASK        (u32)(0x2)
#define MLB_MAILBOX_SYSCONFIG_SOFT_RESET_OFFSET      (u32)(1)
#define MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_MASK          (u32)(0x1)
#define MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_OFFSET        (u32)(0)
#define MLB_MAILBOX_SYSSTATUS_RESET_DONE_MASK         (u32)(0x1)
#define MLB_MAILBOX_SYSSTATUS_RESET_DONE_OFFSET         (u32)(0)
#define MLB_MAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_MASK   (u32)(0x1)
#define MLB_MAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_OFFSET  (u32)(0)
#define MLB_MAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_MASK    (u32)(0x7f)
#define MLB_MAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_OFFSET    (u32)(0)

#endif /* _MLB_ACC_INT_H */
