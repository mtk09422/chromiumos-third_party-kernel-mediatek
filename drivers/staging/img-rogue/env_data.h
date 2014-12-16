/*************************************************************************/ /*!
@File
@Title          Environmental Data header file
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Linux-specific part of system data.
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef _ENV_DATA_
#define _ENV_DATA_

#include <linux/interrupt.h>
#include <linux/pci.h>

#if defined(PVR_LINUX_MISR_USING_WORKQUEUE) || defined(PVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE)
#include <linux/workqueue.h>
#endif

#endif /* _ENV_DATA_ */
/*****************************************************************************
 End of file (env_data.h)
*****************************************************************************/
