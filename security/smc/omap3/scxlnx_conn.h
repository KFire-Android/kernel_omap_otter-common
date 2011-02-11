/*
 *Copyright (c)2006-2008 Trusted Logic S.A.
 *All Rights Reserved.
 *
 *This program is free software; you can redistribute it and/or
 *modify it under the terms of the GNU General Public License
 *version 2 as published by the Free Software Foundation.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *MA 02111-1307 USA
 */

#ifndef __SCXLNX_CONN_H__
#define __SCXLNX_CONN_H__

#include "scxlnx_defs.h"

/*
 *Returns a pointer to the monitor of the connection referenced by the
 *specified file.
 */
static inline SCXLNX_CONN_MONITOR *SCXLNXConnFromFile(struct file *file)
{
	return file->private_data;
}

/*----------------------------------------------------------------------------
 *Connection operations to the SM
 *--------------------------------------------------------------------------*/

int SCXLNXConnCreateDeviceContext(SCXLNX_CONN_MONITOR *pConn,
				  SCX_COMMAND_MESSAGE *pMessage,
				  SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnDestroyDeviceContext(SCXLNX_CONN_MONITOR *pConn);

int SCXLNXConnOpenClientSession(SCXLNX_CONN_MONITOR *pConn,
				SCX_COMMAND_MESSAGE *pMessage,
				SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCloseClientSession(SCXLNX_CONN_MONITOR *pConn,
				 SCX_COMMAND_MESSAGE *pMessage,
				 SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnRegisterSharedMemory(SCXLNX_CONN_MONITOR *pConn,
					SCX_COMMAND_MESSAGE *pMessage,
					SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnReleaseSharedMemory(SCXLNX_CONN_MONITOR *pConn,
				  SCX_COMMAND_MESSAGE *pMessage,
				  SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnInvokeClientCommand(SCXLNX_CONN_MONITOR *pConn,
				  SCX_COMMAND_MESSAGE *pMessage,
				  SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCancelClientCommand(SCXLNX_CONN_MONITOR *pConn,
				  SCX_COMMAND_MESSAGE *pMessage,
				  SCX_ANSWER_MESSAGE *pAnswer);

int SCXLNXConnCheckMessageValidity(SCXLNX_CONN_MONITOR *pConn,
					SCX_COMMAND_MESSAGE *pMessage);

/*----------------------------------------------------------------------------
 *Connection initialization and cleanup operations
 *--------------------------------------------------------------------------*/

int SCXLNXConnOpen(SCXLNX_DEVICE_MONITOR *pDevice,
			struct file *file, SCXLNX_CONN_MONITOR **ppConn);

void SCXLNXConnClose(SCXLNX_CONN_MONITOR *pConn);

/*----------------------------------------------------------------------------
 *Shared memory mapping verification operations
 *--------------------------------------------------------------------------*/

u32 SCXLNXConnCheckSharedMem(SCXLNX_SHMEM_MONITOR *pShmemMonitor,
				  u8 *pBuffer, u32 nBufferSize);

#endif				/*!defined(__SCXLNX_CONN_H__) */
