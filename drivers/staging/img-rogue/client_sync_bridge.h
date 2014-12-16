/*************************************************************************/ /*!
@Title          Direct client bridge header for sync
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef CLIENT_SYNC_BRIDGE_H
#define CLIENT_SYNC_BRIDGE_H

#include "common_sync_bridge.h"

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeAllocSyncPrimitiveBlock(IMG_HANDLE hBridge,
								     IMG_HANDLE hDevNode,
								     IMG_HANDLE *phSyncHandle,
								     IMG_UINT32 *pui32SyncPrimVAddr,
								     IMG_UINT32 *pui32SyncPrimBlockSize,
								     DEVMEM_SERVER_EXPORTCOOKIE *phExportCookie);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeFreeSyncPrimitiveBlock(IMG_HANDLE hBridge,
								    IMG_HANDLE hSyncHandle);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimSet(IMG_HANDLE hBridge,
							 IMG_HANDLE hSyncHandle,
							 IMG_UINT32 ui32Index,
							 IMG_UINT32 ui32Value);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeServerSyncPrimSet(IMG_HANDLE hBridge,
							       IMG_HANDLE hSyncHandle,
							       IMG_UINT32 ui32Value);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncRecordRemoveByHandle(IMG_HANDLE hBridge,
								      IMG_HANDLE hhRecord);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncRecordAdd(IMG_HANDLE hBridge,
							   IMG_HANDLE *phhRecord,
							   IMG_HANDLE hhServerSyncPrimBlock,
							   IMG_UINT32 ui32ui32FwBlockAddr,
							   IMG_UINT32 ui32ui32SyncOffset,
							   IMG_BOOL bbServerSync,
							   IMG_UINT32 ui32ClassNameSize,
							   const IMG_CHAR *puiClassName);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeServerSyncAlloc(IMG_HANDLE hBridge,
							     IMG_HANDLE hDevNode,
							     IMG_HANDLE *phSyncHandle,
							     IMG_UINT32 *pui32SyncPrimVAddr,
							     IMG_UINT32 ui32ClassNameSize,
							     const IMG_CHAR *puiClassName);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeServerSyncFree(IMG_HANDLE hBridge,
							    IMG_HANDLE hSyncHandle);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeServerSyncQueueHWOp(IMG_HANDLE hBridge,
								 IMG_HANDLE hSyncHandle,
								 IMG_BOOL bbUpdate,
								 IMG_UINT32 *pui32FenceValue,
								 IMG_UINT32 *pui32UpdateValue);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeServerSyncGetStatus(IMG_HANDLE hBridge,
								 IMG_UINT32 ui32SyncCount,
								 IMG_HANDLE *phSyncHandle,
								 IMG_UINT32 *pui32UID,
								 IMG_UINT32 *pui32FWAddr,
								 IMG_UINT32 *pui32CurrentOp,
								 IMG_UINT32 *pui32NextOp);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpCreate(IMG_HANDLE hBridge,
							      IMG_UINT32 ui32SyncBlockCount,
							      IMG_HANDLE *phBlockList,
							      IMG_UINT32 ui32ClientSyncCount,
							      IMG_UINT32 *pui32SyncBlockIndex,
							      IMG_UINT32 *pui32Index,
							      IMG_UINT32 ui32ServerSyncCount,
							      IMG_HANDLE *phServerSync,
							      IMG_HANDLE *phServerCookie);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpTake(IMG_HANDLE hBridge,
							    IMG_HANDLE hServerCookie,
							    IMG_UINT32 ui32ClientSyncCount,
							    IMG_UINT32 *pui32Flags,
							    IMG_UINT32 *pui32FenceValue,
							    IMG_UINT32 *pui32UpdateValue,
							    IMG_UINT32 ui32ServerSyncCount,
							    IMG_UINT32 *pui32ServerFlags);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpReady(IMG_HANDLE hBridge,
							     IMG_HANDLE hServerCookie,
							     IMG_BOOL *pbReady);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpComplete(IMG_HANDLE hBridge,
								IMG_HANDLE hServerCookie);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpDestroy(IMG_HANDLE hBridge,
							       IMG_HANDLE hServerCookie);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimPDump(IMG_HANDLE hBridge,
							   IMG_HANDLE hSyncHandle,
							   IMG_UINT32 ui32Offset);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimPDumpValue(IMG_HANDLE hBridge,
								IMG_HANDLE hSyncHandle,
								IMG_UINT32 ui32Offset,
								IMG_UINT32 ui32Value);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimPDumpPol(IMG_HANDLE hBridge,
							      IMG_HANDLE hSyncHandle,
							      IMG_UINT32 ui32Offset,
							      IMG_UINT32 ui32Value,
							      IMG_UINT32 ui32Mask,
							      PDUMP_POLL_OPERATOR eOperator,
							      PDUMP_FLAGS_T uiPDumpFlags);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimOpPDumpPol(IMG_HANDLE hBridge,
								IMG_HANDLE hServerCookie,
								PDUMP_POLL_OPERATOR eOperator,
								PDUMP_FLAGS_T uiPDumpFlags);

IMG_INTERNAL PVRSRV_ERROR IMG_CALLCONV BridgeSyncPrimPDumpCBP(IMG_HANDLE hBridge,
							      IMG_HANDLE hSyncHandle,
							      IMG_UINT32 ui32Offset,
							      IMG_DEVMEM_OFFSET_T uiWriteOffset,
							      IMG_DEVMEM_SIZE_T uiPacketSize,
							      IMG_DEVMEM_SIZE_T uiBufferSize);


#endif /* CLIENT_SYNC_BRIDGE_H */
