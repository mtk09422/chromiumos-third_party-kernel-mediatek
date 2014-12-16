/*************************************************************************/ /*!
@File
@Title          KM server Transport Layer implementation
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Main bridge APIs for Transport Layer client functions
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef __TLSERVER_H_
#define __TLSERVER_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>

#include "img_defs.h"
#include "pvr_debug.h"
#include "connection_server.h"

#include "tlintern.h"

/*
 * Transport Layer Client API Kernel-Mode bridge implementation
 */

PVRSRV_ERROR TLServerConnectKM(CONNECTION_DATA *psConnection);
PVRSRV_ERROR TLServerDisconnectKM(CONNECTION_DATA *psConnection);

PVRSRV_ERROR TLServerOpenStreamKM(IMG_PCHAR pszName,
			   IMG_UINT32 ui32Mode,
			   PTL_STREAM_DESC* ppsSD,
			   DEVMEM_EXPORTCOOKIE** ppsBufCookie);

PVRSRV_ERROR TLServerCloseStreamKM(PTL_STREAM_DESC psSD);

PVRSRV_ERROR TLServerAcquireDataKM(PTL_STREAM_DESC psSD,
			   IMG_UINT32* puiReadOffset,
			   IMG_UINT32* puiReadLen);

PVRSRV_ERROR TLServerReleaseDataKM(PTL_STREAM_DESC psSD,
				 IMG_UINT32 uiReadOffset,
				 IMG_UINT32 uiReadLen);

/*
 * TEST INTERNAL ONLY
 */

PVRSRV_ERROR TLServerTestIoctlKM(IMG_UINT32  uiCmd,
				IMG_PBYTE   uiIn1,
				IMG_UINT32  uiIn2,
				IMG_UINT32*	puiOut1,
				IMG_UINT32* puiOut2);

#if defined (__cplusplus)
}
#endif

#endif /* __TLSERVER_H_ */

/*****************************************************************************
 End of file (tlserver.h)
*****************************************************************************/

