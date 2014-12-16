/*************************************************************************/ /*!
@File
@Title          PVR Bridge IO Functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the PVR Bridge code
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef __PVR_BRIDGE_IO_H__
#define __PVR_BRIDGE_IO_H__

#if defined (__cplusplus)
extern "C" {
#endif

#include <linux/ioctl.h>
#include "pvrsrv_error.h"
  
/*!< Nov 2006: according to ioctl-number.txt 'g' wasn't in use. */
#define PVRSRV_IOC_GID      'g'
#define PVRSRV_IO(INDEX)    _IO(PVRSRV_IOC_GID, INDEX, PVRSRV_BRIDGE_PACKAGE)
#define PVRSRV_IOW(INDEX)   _IOW(PVRSRV_IOC_GID, INDEX, PVRSRV_BRIDGE_PACKAGE)
#define PVRSRV_IOR(INDEX)   _IOR(PVRSRV_IOC_GID, INDEX, PVRSRV_BRIDGE_PACKAGE)
#define PVRSRV_IOWR(INDEX)  _IOWR(PVRSRV_IOC_GID, INDEX, PVRSRV_BRIDGE_PACKAGE)

#if defined (__cplusplus)
}
#endif

#endif /* __PVR_BRIDGE_IO_H__ */

/******************************************************************************
 End of file (pvr_bridge_io.h)
******************************************************************************/
