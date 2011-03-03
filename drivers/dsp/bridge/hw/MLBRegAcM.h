/*
 * MLBRegAcM.h
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

#ifndef _MLB_REG_ACM_H
#define _MLB_REG_ACM_H

#include <GlobalTypes.h>
#include <linux/io.h>
#include <EasiGlobal.h>
#include "MLBAccInt.h"

#if defined(USE_LEVEL_1_MACROS)

#define MLBMAILBOX_SYSCONFIG_READ_REGISTER32(baseAddress)		\
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIG_READ_REGISTER32), \
	__raw_readl(((baseAddress)) + MLB_MAILBOX_SYSCONFIG_OFFSET))

#define MLBMAILBOX_SYSCONFIG_WRITE_REGISTER32(baseAddress, value)	\
do {									\
	const u32 offset = MLB_MAILBOX_SYSCONFIG_OFFSET;		\
	register u32 newValue = ((u32)(value));				\
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIG_WRITE_REGISTER32);\
	__raw_writel(newValue, ((baseAddress)) + offset);		\
} while (0)

#define MLBMAILBOX_SYSCONFIGS_IDLE_MODE_READ32(baseAddress)		  \
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIGS_IDLE_MODE_READ32), \
	(((__raw_readl((((u32)(baseAddress)) +				  \
	(MLB_MAILBOX_SYSCONFIG_OFFSET)))) &				  \
	MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_MASK) >>			  \
	MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_OFFSET))

#define MLBMAILBOX_SYSCONFIGS_IDLE_MODE_WRITE32(baseAddress, value)	  \
do {									  \
	const u32 offset = MLB_MAILBOX_SYSCONFIG_OFFSET;		  \
	register u32 data = __raw_readl(((u32)(baseAddress)) + offset);	  \
	register u32 newValue = ((u32)(value));				  \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIGS_IDLE_MODE_WRITE32); \
	data &= ~(MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_MASK);		  \
	newValue <<= MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_OFFSET;		  \
	newValue &= MLB_MAILBOX_SYSCONFIG_S_IDLE_MODE_MASK;		  \
	newValue |= data;						  \
	__raw_writel(newValue, (u32)(baseAddress) + offset);		  \
} while (0)

#define MLBMAILBOX_SYSCONFIG_SOFT_RESET_WRITE32(baseAddress, value)	    \
do {									    \
	const u32 offset = MLB_MAILBOX_SYSCONFIG_OFFSET;		    \
	register u32 data = __raw_readl(((u32)(baseAddress)) + offset);	    \
	register u32 newValue = ((u32)(value));				    \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIG_SOFT_RESET_WRITE32);   \
	data &= ~(MLB_MAILBOX_SYSCONFIG_SOFT_RESET_MASK);		    \
	newValue <<= MLB_MAILBOX_SYSCONFIG_SOFT_RESET_OFFSET;		    \
	newValue &= MLB_MAILBOX_SYSCONFIG_SOFT_RESET_MASK;		    \
	newValue |= data;						    \
	__raw_writel(newValue, (u32)(baseAddress) + offset);		    \
} while (0)

#define MLBMAILBOX_SYSCONFIG_AUTO_IDLE_READ32(baseAddress)		\
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIG_AUTO_IDLE_READ32), \
	(((__raw_readl((((u32)(baseAddress)) +				 \
	(MLB_MAILBOX_SYSCONFIG_OFFSET)))) &				 \
	  MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_MASK) >>			\
	MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_OFFSET))

#define MLBMAILBOX_SYSCONFIG_AUTO_IDLE_WRITE32(baseAddress, value)	\
do {									 \
	const u32 offset = MLB_MAILBOX_SYSCONFIG_OFFSET;		 \
	register u32 data = __raw_readl(((u32)(baseAddress)) + offset);	 \
	register u32 newValue = ((u32)(value));				 \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSCONFIG_AUTO_IDLE_WRITE32); \
	data &= ~(MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_MASK);		\
	newValue <<= MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_OFFSET;		 \
	newValue &= MLB_MAILBOX_SYSCONFIG_AUTO_IDLE_MASK;		 \
	newValue |= data;						 \
	__raw_writel(newValue, (u32)(baseAddress)+offset);		 \
} while (0)

#define MLBMAILBOX_SYSSTATUS_RESET_DONE_READ32(baseAddress)\
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_SYSSTATUS_RESET_DONE_READ32), \
	(((__raw_readl((((u32)(baseAddress)) +				  \
	(MLB_MAILBOX_SYSSTATUS_OFFSET)))) &				  \
	MLB_MAILBOX_SYSSTATUS_RESET_DONE_MASK) >>			  \
	MLB_MAILBOX_SYSSTATUS_RESET_DONE_OFFSET))

#define MLBMAILBOX_MESSAGE015_READ_REGISTER32(baseAddress, bank)	      \
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_MESSAGE015_READ_REGISTER32), \
	__raw_readl(((baseAddress)) +					      \
	(MLB_MAILBOX_MESSAGE_REGSET015_OFFSET +			      \
	MLB_MAILBOX_MESSAGE015_OFFSET +				      \
	((bank) * MLB_MAILBOX_MESSAGE_REGSET015_STEP))))

#define MLBMAILBOX_MESSAGE015_WRITE_REGISTER32(baseAddress, bank, value)    \
do {									      \
	const u32 offset = MLB_MAILBOX_MESSAGE_REGSET015_OFFSET +	      \
	MLB_MAILBOX_MESSAGE015_OFFSET +				      \
	((bank) * MLB_MAILBOX_MESSAGE_REGSET015_STEP);		      \
	register u32 newValue = ((u32)(value));				      \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_MESSAGE015_WRITE_REGISTER32); \
	__raw_writel(newValue, ((baseAddress)) + offset);		      \
} while (0)

#define MLBMAILBOX_FIFOSTATUS015_READ_REGISTER32(baseAddress, bank)	\
	(_DEBUG_LEVEL1_EASI						\
	(EASIL1_MLBMAILBOX_FIFOSTATUS015_READ_REGISTER32),		\
	__raw_readl(((u32)(baseAddress)) +				\
	(MLB_MAILBOX_FIFOSTATUS_REGSET015_OFFSET +			\
	MLB_MAILBOX_FIFOSTATUS015_OFFSET +				\
	((bank) * MLB_MAILBOX_FIFOSTATUS_REGSET015_STEP))))

#define MLBMAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_READ32(baseAddress, bank) \
	(_DEBUG_LEVEL1_EASI(						 \
	EASIL1_MLBMAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_READ32),		 \
	(((__raw_readl(((baseAddress)) +				 \
	(MLB_MAILBOX_FIFOSTATUS_REGSET015_OFFSET +			 \
	MLB_MAILBOX_FIFOSTATUS015_OFFSET +				 \
	((bank) * MLB_MAILBOX_FIFOSTATUS_REGSET015_STEP)))) &	 \
	MLB_MAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_MASK) >>		 \
	MLB_MAILBOX_FIFOSTATUS015_FIFO_FULL_M_BM_OFFSET))

#define MLBMAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_READ32(baseAddress, bank) \
	(_DEBUG_LEVEL1_EASI(						\
	EASIL1_MLBMAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_READ32),		\
	(((__raw_readl(((baseAddress)) +				\
	(MLB_MAILBOX_MSGSTATUS_REGSET015_OFFSET +			\
	MLB_MAILBOX_MSGSTATUS015_OFFSET +				\
	((bank) * MLB_MAILBOX_MSGSTATUS_REGSET015_STEP)))) &	\
	MLB_MAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_MASK) >>		\
	MLB_MAILBOX_MSGSTATUS015_NB_OF_MSG_M_BM_OFFSET))

#define MLBMAILBOX_IRQSTATUS03_READ_REGISTER32(baseAddress, bank)	       \
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_IRQSTATUS03_READ_REGISTER32), \
	__raw_readl(((baseAddress)) +					       \
	(MLB_MAILBOX_IRQSTATUS_REGSET03_OFFSET +			       \
	MLB_MAILBOX_IRQSTATUS03_OFFSET +				       \
	((bank) * MLB_MAILBOX_IRQSTATUS_REGSET03_STEP))))

#define MLBMAILBOX_IRQSTATUS03_WRITE_REGISTER32(baseAddress, bank, value)    \
do {									       \
	const u32 offset = MLB_MAILBOX_IRQSTATUS_REGSET03_OFFSET +	       \
	MLB_MAILBOX_IRQSTATUS03_OFFSET +				       \
	((bank) * MLB_MAILBOX_IRQSTATUS_REGSET03_STEP);		       \
	register u32 newValue = ((u32)(value));				       \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_IRQSTATUS03_WRITE_REGISTER32); \
	__raw_writel(newValue, ((baseAddress)) + offset);		       \
} while (0)

#define MLBMAILBOX_IRQENABLE03_READ_REGISTER32(baseAddress, bank)	       \
	(_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_IRQENABLE03_READ_REGISTER32), \
	__raw_readl(((baseAddress)) +					       \
	(MLB_MAILBOX_IRQENABLE_REGSET03_OFFSET +			       \
	MLB_MAILBOX_IRQENABLE03_OFFSET +				       \
	((bank) * MLB_MAILBOX_IRQENABLE_REGSET03_STEP))))

#define MLBMAILBOX_IRQENABLE03_WRITE_REGISTER32(baseAddress, bank, value)    \
do {									       \
	const u32 offset = MLB_MAILBOX_IRQENABLE_REGSET03_OFFSET +	       \
	MLB_MAILBOX_IRQENABLE03_OFFSET +				       \
	((bank)*MLB_MAILBOX_IRQENABLE_REGSET03_STEP);		       \
	register u32 newValue = ((u32)(value));				       \
	_DEBUG_LEVEL1_EASI(EASIL1_MLBMAILBOX_IRQENABLE03_WRITE_REGISTER32); \
	__raw_writel(newValue, ((baseAddress)) + offset);		       \
} while (0)

#endif /* USE_LEVEL_1_MACROS */

#endif /* _MLB_REG_ACM_H */
