/*************************************************************************/ /*!
@Title          Direct client bridge for pvrtl
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "client_pvrtl_bridge.h"
#include "img_defs.h"
#include "pvr_debug.h"

/* Module specific includes */
#include "devicemem_typedefs.h"
#include "pvr_tl.h"
#include "tltestdefs.h"

#include "tlserver.h"


IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLConnect(IMG_HANDLE hBridge)
{
	PVRSRV_ERROR eError;


	eError =
		TLServerConnectKM(hBridge
					);
	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLDisconnect(IMG_HANDLE hBridge)
{
	PVRSRV_ERROR eError;


	eError =
		TLServerDisconnectKM(hBridge
					);
	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLOpenStream(IMG_HANDLE hBridge,
							  IMG_CHAR *puiName,
							  IMG_UINT32 ui32Mode,
							  IMG_HANDLE *phSD,
							  DEVMEM_SERVER_EXPORTCOOKIE *phClientBUFExportCookie)
{
	PVRSRV_ERROR eError;
	TL_STREAM_DESC * psSDInt;
	DEVMEM_EXPORTCOOKIE * psClientBUFExportCookieInt;
	PVR_UNREFERENCED_PARAMETER(hBridge);


	eError =
		TLServerOpenStreamKM(
					puiName,
					ui32Mode,
					&psSDInt,
					&psClientBUFExportCookieInt);

	*phSD = psSDInt;
	*phClientBUFExportCookie = psClientBUFExportCookieInt;
	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLCloseStream(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD)
{
	PVRSRV_ERROR eError;
	TL_STREAM_DESC * psSDInt;
	PVR_UNREFERENCED_PARAMETER(hBridge);

	psSDInt = (TL_STREAM_DESC *) hSD;

	eError =
		TLServerCloseStreamKM(
					psSDInt);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLAcquireData(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD,
							   IMG_UINT32 *pui32ReadOffset,
							   IMG_UINT32 *pui32ReadLen)
{
	PVRSRV_ERROR eError;
	TL_STREAM_DESC * psSDInt;
	PVR_UNREFERENCED_PARAMETER(hBridge);

	psSDInt = (TL_STREAM_DESC *) hSD;

	eError =
		TLServerAcquireDataKM(
					psSDInt,
					pui32ReadOffset,
					pui32ReadLen);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLReleaseData(IMG_HANDLE hBridge,
							   IMG_HANDLE hSD,
							   IMG_UINT32 ui32ReadOffset,
							   IMG_UINT32 ui32ReadLen)
{
	PVRSRV_ERROR eError;
	TL_STREAM_DESC * psSDInt;
	PVR_UNREFERENCED_PARAMETER(hBridge);

	psSDInt = (TL_STREAM_DESC *) hSD;

	eError =
		TLServerReleaseDataKM(
					psSDInt,
					ui32ReadOffset,
					ui32ReadLen);

	return eError;
}

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeTLTestIoctl(IMG_HANDLE hBridge,
							 IMG_UINT32 ui32Cmd,
							 IMG_BYTE *psIn1,
							 IMG_UINT32 ui32In2,
							 IMG_UINT32 *pui32Out1,
							 IMG_UINT32 *pui32Out2)
{
	PVRSRV_ERROR eError;
	PVR_UNREFERENCED_PARAMETER(hBridge);


	eError =
		TLServerTestIoctlKM(
					ui32Cmd,
					psIn1,
					ui32In2,
					pui32Out1,
					pui32Out2);

	return eError;
}

