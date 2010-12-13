/*
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __SCXLNX_CONN_H__
#define __SCXLNX_CONN_H__

#include "scxlnx_defs.h"

/*
 * Returns a pointer to the connection referenced by the
 * specified file.
 */
static inline struct SCXLNX_CONNECTION *SCXLNXConnFromFile(
	struct file *file)
{
	return file->private_data;
}

/*----------------------------------------------------------------------------
 * Connection operations to the Secure World
 *----------------------------------------------------------------------------*/

int SCXLNXConnCreateDeviceContext(
	 struct SCXLNX_CONNECTION *pConn);

int SCXLNXConnDestroyDeviceContext(
	struct SCXLNX_CONNECTION *pConn);

int SCXLNXConnOpenClientSession(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCloseClientSession(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnRegisterSharedMemory(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnReleaseSharedMemory(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnInvokeClientCommand(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCancelClientCommand(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage,
	union SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCheckMessageValidity(
	struct SCXLNX_CONNECTION *pConn,
	union SCX_COMMAND_MESSAGE *pMessage);

/*----------------------------------------------------------------------------
 * Connection initialization and cleanup operations
 *----------------------------------------------------------------------------*/

int SCXLNXConnOpen(struct SCXLNX_DEVICE *pDevice,
	struct file *file,
	struct SCXLNX_CONNECTION **ppConn);

void SCXLNXConnClose(
	struct SCXLNX_CONNECTION *pConn);


#endif  /* !defined(__SCXLNX_CONN_H__) */
