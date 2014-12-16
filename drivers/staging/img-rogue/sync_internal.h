/*************************************************************************/ /*!
@File
@Title          Services internal synchronisation interface header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines the internal client side interface for services
                synchronisation
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _SYNC_INTERNAL_
#define _SYNC_INTERNAL_

#include "img_types.h"
#include "sync_external.h"
#include "ra.h"
#include "dllist.h"
#include "lock.h"
#include "devicemem.h"

/*
	Private structure's
*/
#define SYNC_PRIM_NAME_SIZE		50
typedef struct SYNC_PRIM_CONTEXT
{
	SYNC_BRIDGE_HANDLE			hBridge;						/*!< Bridge handle */
	IMG_HANDLE					hDeviceNode;					/*!< The device we're operating on */
	IMG_CHAR					azName[SYNC_PRIM_NAME_SIZE];	/*!< Name of the RA */
	RA_ARENA					*psSubAllocRA;					/*!< RA context */
	IMG_CHAR					azSpanName[SYNC_PRIM_NAME_SIZE];/*!< Name of the span RA */
	RA_ARENA					*psSpanRA;						/*!< RA used for span management of SubAllocRA */
	IMG_UINT32					ui32RefCount;					/*!< Refcount for this context */
	POS_LOCK					hLock;							/*!< Lock for this context */
} SYNC_PRIM_CONTEXT;

typedef struct _SYNC_PRIM_BLOCK_
{
	SYNC_PRIM_CONTEXT	*psContext;				/*!< Our copy of the services connection */
	IMG_HANDLE			hServerSyncPrimBlock;	/*!< Server handle for this block */
	IMG_UINT32			ui32SyncBlockSize;		/*!< Size of the sync prim block */
	IMG_UINT32			ui32FirmwareAddr;		/*!< Firmware address */
	DEVMEM_MEMDESC		*hMemDesc;				/*!< Host mapping handle */
	IMG_UINT32			*pui32LinAddr;			/*!< User CPU mapping */
	IMG_UINT64			uiSpanBase;				/*!< Base of this import in the span RA */
	DLLIST_NODE			sListNode;				/*!< List node for the sync block list */
} SYNC_PRIM_BLOCK;

typedef enum _SYNC_PRIM_TYPE_
{
	SYNC_PRIM_TYPE_UNKNOWN = 0,
	SYNC_PRIM_TYPE_LOCAL,
	SYNC_PRIM_TYPE_SERVER,
} SYNC_PRIM_TYPE;

typedef struct _SYNC_PRIM_LOCAL_
{
	SYNC_PRIM_BLOCK			*psSyncBlock;	/*!< Synchronisation block this primitive is allocated on */
	IMG_UINT64				uiSpanAddr;		/*!< Span address of the sync */
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	IMG_HANDLE				hRecord;		/*!< Sync record handle */
#endif
} SYNC_PRIM_LOCAL;

typedef struct _SYNC_PRIM_SERVER_
{
	SYNC_BRIDGE_HANDLE		hBridge;			/*!< Bridge handle */
	IMG_HANDLE				hServerSync;		/*!< Handle to the server sync */
	IMG_UINT32				ui32FirmwareAddr;	/*!< Firmware address of the sync */
} SYNC_PRIM_SERVER;

typedef struct _SYNC_PRIM_
{
	PVRSRV_CLIENT_SYNC_PRIM	sCommon;		/*!< Client visible part of the sync prim */
	SYNC_PRIM_TYPE			eType;			/*!< Sync primative type */
	union {
		SYNC_PRIM_LOCAL		sLocal;			/*!< Local sync primative data */
		SYNC_PRIM_SERVER	sServer;		/*!< Server sync primative data */
	} u;
} SYNC_PRIM;


/* FIXME this must return a correctly typed pointer */
IMG_INTERNAL IMG_UINT32 SyncPrimGetFirmwareAddr(PVRSRV_CLIENT_SYNC_PRIM *psSync);

#endif	/* _SYNC_INTERNAL_ */
