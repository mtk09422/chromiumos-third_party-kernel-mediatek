/*************************************************************************/ /*!
@File
@Title          Linux private data structure
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__INCLUDED_PRIVATE_DATA_H_)
#define __INCLUDED_PRIVATE_DATA_H_

#include <linux/fs.h>

#include "connection_server.h"

CONNECTION_DATA *LinuxConnectionFromFile(struct file *pFile);
struct file *LinuxFileFromConnection(CONNECTION_DATA *psConnection);

#endif /* !defined(__INCLUDED_PRIVATE_DATA_H_) */
