/*************************************************************************/ /*!
@File
@Title          Services external synchronisation interface header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines synchronisation structures that are visible internally
                and externally
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "img_types.h"

#ifndef _SYNC_EXTERNAL_
#define _SYNC_EXTERNAL_

#define SYNC_MAX_CLASS_NAME_LEN 32

typedef IMG_HANDLE SYNC_BRIDGE_HANDLE;
typedef struct SYNC_PRIM_CONTEXT *PSYNC_PRIM_CONTEXT;
typedef struct _SYNC_OP_COOKIE_ *PSYNC_OP_COOKIE;

typedef struct PVRSRV_CLIENT_SYNC_PRIM
{
	volatile IMG_UINT32	*pui32LinAddr;	/*!< User pointer to the primitive */
} PVRSRV_CLIENT_SYNC_PRIM;

typedef IMG_HANDLE PVRSRV_CLIENT_SYNC_PRIM_HANDLE;

typedef struct PVRSRV_CLIENT_SYNC_PRIM_OP
{
	IMG_UINT32 					ui32Flags;				/*!< Operation flags */
#define PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK	(1 << 0)
#define PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE	(1 << 1)
#define PVRSRV_CLIENT_SYNC_PRIM_OP_UNFENCED_UPDATE (PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE | (1<<2))
	PVRSRV_CLIENT_SYNC_PRIM		*psSync;				/*!< Pointer to the client sync */
	IMG_UINT32					ui32FenceValue;			/*!< The Fence value (only used if PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK is set) */
	IMG_UINT32					ui32UpdateValue;		/*!< The Update value (only used if PVRSRV_CLIENT_SYNC_PRIM_OP_UPDATE is set) */
} PVRSRV_CLIENT_SYNC_PRIM_OP;

#if defined(__KERNEL__) && defined(ANDROID) && !defined(__GENKSYMS__)
#define __pvrsrv_defined_struct_enum__
#include <services_kernel_client.h>
#endif

#endif /* _SYNC_EXTERNAL_ */
