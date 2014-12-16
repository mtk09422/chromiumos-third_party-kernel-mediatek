/*************************************************************************/ /*!
@File
@Title          Client bridge header for pvrtl
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Exports the client bridge functions for pvrtl
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef CLIENT_PVRTL_BRIDGE_H
#define CLIENT_PVRTL_BRIDGE_H

#include "pvr_bridge_client.h"
#include "pvr_bridge.h"

#include "common_pvrtl_bridge.h"

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLConnect(IMG_HANDLE hBridge);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLDisconnect(IMG_HANDLE hBridge);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLOpenStream(IMG_HANDLE hBridge,
							  IMG_CHAR *puiName,
							  IMG_UINT32 ui32Mode,
							  IMG_HANDLE *phSD,
							  DEVMEM_SERVER_EXPORTCOOKIE *phClientBUFExportCookie);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLCloseStream(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLAcquireData(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD,
							   IMG_UINT32 *pui32ReadOffset,
							   IMG_UINT32 *pui32ReadLen);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLReleaseData(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD,
							   IMG_UINT32 ui32ReadOffset,
							   IMG_UINT32 ui32ReadLen);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLTestIoctl(IMG_HANDLE hBridge,
							 IMG_UINT32 ui32Cmd,
							 IMG_BYTE *psIn1,
							 IMG_UINT32 ui32In2,
							 IMG_UINT32 *pui32Out1,
							 IMG_UINT32 *pui32Out2);


#endif /* CLIENT_PVRTL_BRIDGE_H */
