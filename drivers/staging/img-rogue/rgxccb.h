/*************************************************************************/ /*!
@File
@Title          RGX Circular Command Buffer functionality.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the RGX Circular Command Buffer functionality.
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__RGXCCB_H__)
#define __RGXCCB_H__

#include "devicemem.h"
#include "device.h"
#include "rgxdevice.h"
#include "sync_server.h"
#include "connection_server.h"
#include "rgx_fwif_shared.h"
#include "rgxdebug.h"

#define MAX_CLIENT_CCB_NAME	30

typedef struct _RGX_CLIENT_CCB_ RGX_CLIENT_CCB;

/*
	This structure is declared here as it's allocated on the heap by
	the callers
*/

typedef struct _RGX_CCB_CMD_HELPER_DATA_ {
	/* Data setup at command init time */
	RGX_CLIENT_CCB  		*psClientCCB;
	IMG_CHAR 				*pszCommandName;
	IMG_BOOL 				bPDumpContinuous;
	
	IMG_UINT32				ui32ClientFenceCount;
	PRGXFWIF_UFO_ADDR		*pauiFenceUFOAddress;
	IMG_UINT32				*paui32FenceValue;
	IMG_UINT32				ui32ClientUpdateCount;
	PRGXFWIF_UFO_ADDR		*pauiUpdateUFOAddress;
	IMG_UINT32				*paui32UpdateValue;

	IMG_UINT32				ui32ServerSyncCount;
	IMG_UINT32				*paui32ServerSyncFlags;
	SERVER_SYNC_PRIMITIVE	**papsServerSyncs;

	RGXFWIF_CCB_CMD_TYPE	eType;
	IMG_UINT32				ui32CmdSize;
	IMG_UINT8				*pui8DMCmd;
	IMG_UINT32				ui32FenceCmdSize;
	IMG_UINT32				ui32DMCmdSize;
	IMG_UINT32				ui32UpdateCmdSize;
	IMG_UINT32				ui32UnfencedUpdateCmdSize;

	/* timestamp commands */
	RGXFWIF_DEV_VIRTADDR    pPreTimestamp;
	IMG_UINT32              ui32PreTimeStampCmdSize;
	RGXFWIF_DEV_VIRTADDR    pPostTimestamp;
	IMG_UINT32              ui32PostTimeStampCmdSize;
	PRGXFWIF_UFO_ADDR       pRMWUFOAddr;
	IMG_UINT32              ui32RMWUFOCmdSize;

	/* Data setup at command acquire time */
	IMG_UINT8				*pui8StartPtr;
	IMG_UINT8				*pui8ServerUpdateStart;
	IMG_UINT8				*pui8ServerUnfencedUpdateStart;
	IMG_UINT8				*pui8ServerFenceStart;
	IMG_UINT32				ui32ServerFenceCount;
	IMG_UINT32				ui32ServerUpdateCount;
	IMG_UINT32				ui32ServerUnfencedUpdateCount;

} RGX_CCB_CMD_HELPER_DATA;

#define PADDING_COMMAND_SIZE	(sizeof(RGXFWIF_CCB_CMD_HEADER))

PVRSRV_ERROR RGXCreateCCB(PVRSRV_DEVICE_NODE	*psDeviceNode,
						  IMG_UINT32			ui32CCBSizeLog2,
						  CONNECTION_DATA		*psConnectionData,
						  const IMG_CHAR		*pszName,
						  RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
						  RGX_CLIENT_CCB		**ppsClientCCB,
						  DEVMEM_MEMDESC 		**ppsClientCCBMemDesc,
						  DEVMEM_MEMDESC 		**ppsClientCCBCtlMemDesc);

IMG_VOID RGXDestroyCCB(RGX_CLIENT_CCB *psClientCCB);

PVRSRV_ERROR RGXAcquireCCB(RGX_CLIENT_CCB *psClientCCB,
										IMG_UINT32		ui32CmdSize,
										IMG_PVOID		*ppvBufferSpace,
										IMG_BOOL		bPDumpContinuous);

IMG_INTERNAL IMG_VOID RGXReleaseCCB(RGX_CLIENT_CCB *psClientCCB,
									IMG_UINT32		ui32CmdSize,
									IMG_BOOL		bPDumpContinuous);

IMG_UINT32 RGXGetHostWriteOffsetCCB(RGX_CLIENT_CCB *psClientCCB);

PVRSRV_ERROR RGXCmdHelperInitCmdCCB(RGX_CLIENT_CCB          *psClientCCB,
                                    IMG_UINT32              ui32ClientFenceCount,
                                    PRGXFWIF_UFO_ADDR       *pauiFenceUFOAddress,
                                    IMG_UINT32              *paui32FenceValue,
                                    IMG_UINT32              ui32ClientUpdateCount,
                                    PRGXFWIF_UFO_ADDR       *pauiUpdateUFOAddress,
                                    IMG_UINT32              *paui32UpdateValue,
                                    IMG_UINT32              ui32ServerSyncCount,
                                    IMG_UINT32              *paui32ServerSyncFlags,
                                    SERVER_SYNC_PRIMITIVE   **pasServerSyncs,
                                    IMG_UINT32              ui32CmdSize,
                                    IMG_UINT8               *pui8DMCmd,
                                    RGXFWIF_DEV_VIRTADDR    *ppPreTimestamp,
                                    RGXFWIF_DEV_VIRTADDR    *ppPostTimestamp,
                                    RGXFWIF_DEV_VIRTADDR    *ppRMWUFOAddr,
                                    RGXFWIF_CCB_CMD_TYPE    eType,
                                    IMG_BOOL                bPDumpContinuous,
                                    IMG_CHAR                *pszCommandName,
                                    RGX_CCB_CMD_HELPER_DATA *psCmdHelperData);

PVRSRV_ERROR RGXCmdHelperAcquireCmdCCB(IMG_UINT32 ui32CmdCount,
									   RGX_CCB_CMD_HELPER_DATA *asCmdHelperData,
									   IMG_BOOL *pbKickRequired);

IMG_VOID RGXCmdHelperReleaseCmdCCB(IMG_UINT32 ui32CmdCount,
								   RGX_CCB_CMD_HELPER_DATA *asCmdHelperData,
								   const IMG_CHAR *pcszDMName,
								   IMG_UINT32 ui32CtxAddr);

IMG_UINT32 RGXCmdHelperGetCommandSize(IMG_UINT32 ui32CmdCount,
								   RGX_CCB_CMD_HELPER_DATA *asCmdHelperData);

IMG_VOID DumpStalledCCBCommand(PRGXFWIF_FWCOMMONCONTEXT sFWCommonContext, RGX_CLIENT_CCB  *psCurrentClientCCB, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf);

PVRSRV_ERROR CheckForStalledCCB(RGX_CLIENT_CCB  *psCurrentClientCCB);
#endif /* __RGXCCB_H__ */
