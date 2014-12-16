/*************************************************************************/ /*!
@File
@Title          Common bridge header for rgxtq
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for rgxtq
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_RGXTQ_BRIDGE_H
#define COMMON_RGXTQ_BRIDGE_H

#include "rgx_bridge.h"
#include "sync_external.h"
#include "rgx_fwif_shared.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_RGXTQ_CMD_FIRST			(PVRSRV_BRIDGE_RGXTQ_START)
#define PVRSRV_BRIDGE_RGXTQ_RGXCREATETRANSFERCONTEXT			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+0)
#define PVRSRV_BRIDGE_RGXTQ_RGXDESTROYTRANSFERCONTEXT			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+1)
#define PVRSRV_BRIDGE_RGXTQ_RGXSUBMITTRANSFER			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+2)
#define PVRSRV_BRIDGE_RGXTQ_RGXSETTRANSFERCONTEXTPRIORITY			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+3)
#define PVRSRV_BRIDGE_RGXTQ_RGXKICKSYNCTRANSFER			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+4)
#define PVRSRV_BRIDGE_RGXTQ_CMD_LAST			(PVRSRV_BRIDGE_RGXTQ_CMD_FIRST+4)


/*******************************************
            RGXCreateTransferContext          
 *******************************************/

/* Bridge in structure for RGXCreateTransferContext */
typedef struct PVRSRV_BRIDGE_IN_RGXCREATETRANSFERCONTEXT_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32Priority;
	IMG_DEV_VIRTADDR sMCUFenceAddr;
	IMG_UINT32 ui32FrameworkCmdize;
	IMG_BYTE * psFrameworkCmd;
	IMG_HANDLE hPrivData;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCREATETRANSFERCONTEXT;


/* Bridge out structure for RGXCreateTransferContext */
typedef struct PVRSRV_BRIDGE_OUT_RGXCREATETRANSFERCONTEXT_TAG
{
	IMG_HANDLE hTransferContext;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCREATETRANSFERCONTEXT;

/*******************************************
            RGXDestroyTransferContext          
 *******************************************/

/* Bridge in structure for RGXDestroyTransferContext */
typedef struct PVRSRV_BRIDGE_IN_RGXDESTROYTRANSFERCONTEXT_TAG
{
	IMG_HANDLE hTransferContext;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXDESTROYTRANSFERCONTEXT;


/* Bridge out structure for RGXDestroyTransferContext */
typedef struct PVRSRV_BRIDGE_OUT_RGXDESTROYTRANSFERCONTEXT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXDESTROYTRANSFERCONTEXT;

/*******************************************
            RGXSubmitTransfer          
 *******************************************/

/* Bridge in structure for RGXSubmitTransfer */
typedef struct PVRSRV_BRIDGE_IN_RGXSUBMITTRANSFER_TAG
{
	IMG_HANDLE hTransferContext;
	IMG_UINT32 ui32PrepareCount;
	IMG_UINT32 * pui32ClientFenceCount;
	PRGXFWIF_UFO_ADDR* * psFenceUFOAddress;
	IMG_UINT32* * pui32FenceValue;
	IMG_UINT32 * pui32ClientUpdateCount;
	PRGXFWIF_UFO_ADDR* * psUpdateUFOAddress;
	IMG_UINT32* * pui32UpdateValue;
	IMG_UINT32 * pui32ServerSyncCount;
	IMG_UINT32* * pui32ServerSyncFlags;
	IMG_HANDLE* * phServerSync;
	IMG_UINT32 ui32NumFenceFDs;
	IMG_INT32 * pi32FenceFDs;
	IMG_UINT32 * pui32CommandSize;
	IMG_UINT8* * pui8FWCommand;
	IMG_UINT32 * pui32TQPrepareFlags;
	IMG_UINT32 ui32ExternalJobReference;
	IMG_UINT32 ui32InternalJobReference;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXSUBMITTRANSFER;


/* Bridge out structure for RGXSubmitTransfer */
typedef struct PVRSRV_BRIDGE_OUT_RGXSUBMITTRANSFER_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXSUBMITTRANSFER;

/*******************************************
            RGXSetTransferContextPriority          
 *******************************************/

/* Bridge in structure for RGXSetTransferContextPriority */
typedef struct PVRSRV_BRIDGE_IN_RGXSETTRANSFERCONTEXTPRIORITY_TAG
{
	IMG_HANDLE hTransferContext;
	IMG_UINT32 ui32Priority;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXSETTRANSFERCONTEXTPRIORITY;


/* Bridge out structure for RGXSetTransferContextPriority */
typedef struct PVRSRV_BRIDGE_OUT_RGXSETTRANSFERCONTEXTPRIORITY_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXSETTRANSFERCONTEXTPRIORITY;

/*******************************************
            RGXKickSyncTransfer          
 *******************************************/

/* Bridge in structure for RGXKickSyncTransfer */
typedef struct PVRSRV_BRIDGE_IN_RGXKICKSYNCTRANSFER_TAG
{
	IMG_HANDLE hTransferContext;
	IMG_UINT32 ui32ClientFenceCount;
	PRGXFWIF_UFO_ADDR * psClientFenceUFOAddress;
	IMG_UINT32 * pui32ClientFenceValue;
	IMG_UINT32 ui32ClientUpdateCount;
	PRGXFWIF_UFO_ADDR * psClientUpdateUFOAddress;
	IMG_UINT32 * pui32ClientUpdateValue;
	IMG_UINT32 ui32ServerSyncCount;
	IMG_UINT32 * pui32ServerSyncFlags;
	IMG_HANDLE * phServerSyncs;
	IMG_UINT32 ui32NumFenceFDs;
	IMG_INT32 * pi32FenceFDs;
	IMG_UINT32 ui32TQPrepareFlags;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXKICKSYNCTRANSFER;


/* Bridge out structure for RGXKickSyncTransfer */
typedef struct PVRSRV_BRIDGE_OUT_RGXKICKSYNCTRANSFER_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXKICKSYNCTRANSFER;

#endif /* COMMON_RGXTQ_BRIDGE_H */
