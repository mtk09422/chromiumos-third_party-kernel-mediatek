/**************************************************************************/ /*!
@File
@Title          Common Device header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device related function templates and defines
@License        Strictly Confidential.
*/ /***************************************************************************/

#ifndef __DEVICE_H__
#define __DEVICE_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "devicemem_heapcfg.h"
#include "mmu_common.h"	
#include "ra.h"  		/* RA_ARENA */
#include "pvrsrv_device.h"
#include "srvkm.h"
#include "devicemem.h"
#include "physheap.h"
#include "sync.h"
#include "dllist.h"
#include "cache_external.h"

#include "lock.h"

#if defined(SUPPORT_GPUVIRT_VALIDATION)
#include "services.h"
#endif

/* BM context forward reference */
typedef struct _BM_CONTEXT_ BM_CONTEXT;

/*********************************************************************/ /*!
 @Function      AllocUFOCallback
 @Description   Device specific callback for allocation of an UFO block

 @Input         psDeviceNode          Pointer to device node to allocate
                                      the UFO for.
 @Output        ppsMemDesc            Pointer to pointer for the memdesc of
                                      the allocation
 @Output        pui32SyncAddr         FW Base address of the UFO block
 @Output        puiSyncPrimBlockSize  Size of the UFO block

 @Return        PVRSRV_OK if allocation was successful
 */
/*********************************************************************/
typedef PVRSRV_ERROR (*AllocUFOBlockCallback)(struct _PVRSRV_DEVICE_NODE_ *psDeviceNode,
														DEVMEM_MEMDESC **ppsMemDesc,
														IMG_UINT32 *pui32SyncAddr,
														IMG_UINT32 *puiSyncPrimBlockSize);

/*********************************************************************/ /*!
 @Function      FreeUFOCallback
 @Description   Device specific callback for freeing of an UFO

 @Input         psDeviceNode    Pointer to device node that the UFO block was
                                allocated from.
 @Input         psMemDesc       Pointer to pointer for the memdesc of
                                the UFO block to free.
 */
/*********************************************************************/
typedef IMG_VOID (*FreeUFOBlockCallback)(struct _PVRSRV_DEVICE_NODE_ *psDeviceNode,
										 DEVMEM_MEMDESC *psMemDesc);


typedef struct _DEVICE_MEMORY_INFO_
{
	/* size of address space, as log2 */
	IMG_UINT32				ui32AddressSpaceSizeLog2;

	/* 
		flags, includes physical memory resource types available to the system.  
		Allows for validation at heap creation, define PVRSRV_BACKINGSTORE_XXX 
	*/
	IMG_UINT32				ui32Flags;

	/* heap count.  Doesn't include additional heaps from PVRSRVCreateDeviceMemHeap */
	IMG_UINT32				ui32HeapCount;

	/* BM kernel context for the device */
    BM_CONTEXT				*pBMKernelContext;

	/* BM context list for the device*/
    BM_CONTEXT				*pBMContext;

    /* Blueprints for creating new device memory contexts */
    IMG_UINT32              uiNumHeapConfigs;
    DEVMEM_HEAP_CONFIG      *psDeviceMemoryHeapConfigArray;
    DEVMEM_HEAP_BLUEPRINT   *psDeviceMemoryHeap;
} DEVICE_MEMORY_INFO;


typedef struct _Px_HANDLE_
{
	union
	{
		IMG_VOID *pvHandle;
		IMG_UINT64 ui64Handle;
	}u;
} Px_HANDLE;

typedef enum _PVRSRV_DEVICE_STATE_
{
	PVRSRV_DEVICE_STATE_UNDEFINED = 0,
	PVRSRV_DEVICE_STATE_INIT,
	PVRSRV_DEVICE_STATE_ACTIVE,
	PVRSRV_DEVICE_STATE_DEINIT,
} PVRSRV_DEVICE_STATE;

typedef enum _PVRSRV_DEVICE_HEALTH_
{
	PVRSRV_DEVICE_HEALTH_STATUS_OK = 0,
	PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING,
	PVRSRV_DEVICE_HEALTH_STATUS_DEAD
} PVRSRV_DEVICE_HEALTH_STATUS;

typedef PVRSRV_ERROR (*FN_CREATERAMBACKEDPMR)(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
										IMG_DEVMEM_SIZE_T uiSize,
										IMG_DEVMEM_SIZE_T uiChunkSize,
										IMG_UINT32 ui32NumPhysChunks,
										IMG_UINT32 ui32NumVirtChunks,
										IMG_BOOL *pabMappingTable,
										IMG_UINT32 uiLog2PageSize,
										PVRSRV_MEMALLOCFLAGS_T uiFlags,
										PMR **ppsPMRPtr);
typedef struct _PVRSRV_DEVICE_NODE_
{
	PVRSRV_DEVICE_IDENTIFIER	sDevId;

	PVRSRV_DEVICE_STATE			eDevState;
	PVRSRV_DEVICE_HEALTH_STATUS eHealthStatus;

	/* device specific MMU attributes */
    MMU_DEVICEATTRIBS      *psMMUDevAttrs;

	/*
		callbacks the device must support:
	*/

    FN_CREATERAMBACKEDPMR pfnCreateRamBackedPMR[PVRSRV_DEVICE_PHYS_HEAP_LAST];

    PVRSRV_ERROR (*pfnMMUPxAlloc)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, IMG_SIZE_T uiSize,
									Px_HANDLE *psMemHandle, IMG_DEV_PHYADDR *psDevPAddr);

    IMG_VOID (*pfnMMUPxFree)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, Px_HANDLE *psMemHandle);

	PVRSRV_ERROR (*pfnMMUPxMap)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, Px_HANDLE *pshMemHandle,
								IMG_SIZE_T uiSize, IMG_DEV_PHYADDR *psDevPAddr,
								IMG_VOID **pvPtr);

	IMG_VOID (*pfnMMUPxUnmap)(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
								Px_HANDLE *psMemHandle, IMG_VOID *pvPtr);

	IMG_UINT32 uiMMUPxLog2AllocGran;
	IMG_CHAR				*pszMMUPxPDumpMemSpaceName;

	IMG_VOID (*pfnMMUCacheInvalidate)(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
										IMG_HANDLE hDeviceData,
										MMU_LEVEL eLevel,
										IMG_BOOL bUnmap);

	PVRSRV_ERROR (*pfnSLCCacheInvalidateRequest)(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
										PMR *psPmr);

	IMG_VOID (*pfnDumpDebugInfo)(struct _PVRSRV_DEVICE_NODE_ *psDevNode);

	PVRSRV_ERROR (*pfnUpdateHealthStatus)(struct _PVRSRV_DEVICE_NODE_ *psDevNode,
	                                      IMG_BOOL bIsTimerPoll);

	PVRSRV_ERROR (*pfnResetHWRLogs)(struct _PVRSRV_DEVICE_NODE_ *psDevNode);

	/* Method to drain device HWPerf packets from firmware buffer to host buffer */
	PVRSRV_ERROR (*pfnServiceHWPerf)(struct _PVRSRV_DEVICE_NODE_ *psDevNode);

	PVRSRV_ERROR (*pfnDeviceVersionString)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, IMG_CHAR **ppszVersionString);

	PVRSRV_ERROR (*pfnDeviceClockSpeed)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, IMG_PUINT32 pui32RGXClockSpeed);

	PVRSRV_ERROR (*pfnSoftReset)(struct _PVRSRV_DEVICE_NODE_ *psDevNode, IMG_UINT64 ui64ResetValue1, IMG_UINT64 ui64ResetValue2);

	PVRSRV_DEVICE_CONFIG	*psDevConfig;

	/* device post-finalise compatibility check */
	PVRSRV_ERROR			(*pfnInitDeviceCompatCheck) (struct _PVRSRV_DEVICE_NODE_*,IMG_UINT32 ui32ClientBuildOptions);

	/* information about the device's address space and heaps */
	DEVICE_MEMORY_INFO		sDevMemoryInfo;

	/* private device information */
	IMG_VOID				*pvDevice;

	IMG_CHAR				szRAName[50];

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	RA_ARENA                *psOSidSubArena[GPUVIRT_VALIDATION_NUM_OS];
#endif

	RA_ARENA				*psLocalDevMemArena;

	/*
	 * Pointers to the device's physical memory heap(s)
	 * The first entry (apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL]) will be used for allocations
	 *  where the PVRSRV_MEMALLOCFLAG_CPU_LOCAL flag is not set. Normally this will be an LMA heap
	 *  (but the device configuration could specify a UMA heap here, if desired)
	 * The second entry (apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_CPU_LOCAL]) will be used for allocations
	 *  where the PVRSRV_MEMALLOCFLAG_CPU_LOCAL flag is set. Normally this will be a UMA heap
	 *  (but the configuration could specify an LMA heap here, if desired)
	 * The device configuration will always specify two physical heap IDs - in the event of the device
	 *  only using one physical heap, both of these IDs will be the same, and hence both pointers below
	 *  will also be the same
	 */
	PHYS_HEAP				*apsPhysHeap[PVRSRV_DEVICE_PHYS_HEAP_LAST];

	struct _PVRSRV_DEVICE_NODE_	*psNext;
	struct _PVRSRV_DEVICE_NODE_	**ppsThis;
	
	/* Functions for notification about memory contexts */
	PVRSRV_ERROR			(*pfnRegisterMemoryContext)(struct _PVRSRV_DEVICE_NODE_	*psDeviceNode,
														MMU_CONTEXT					*psMMUContext,
														IMG_HANDLE					*hPrivData);
	IMG_VOID				(*pfnUnregisterMemoryContext)(IMG_HANDLE hPrivData);

	/* Funtions for allocation/freeing of UFOs */
	AllocUFOBlockCallback	pfnAllocUFOBlock;	/*!< Callback for allocation of a block of UFO memory */
	FreeUFOBlockCallback	pfnFreeUFOBlock;	/*!< Callback for freeing of a block of UFO memory */

	PSYNC_PRIM_CONTEXT		hSyncPrimContext;

	PVRSRV_CLIENT_SYNC_PRIM *psSyncPrim;

	IMG_HANDLE				hCmdCompNotify;
	IMG_HANDLE				hDbgReqNotify;

#if defined(PDUMP)
	/* 	device-level callback which is called when pdump.exe starts.
	 *	Should be implemented in device-specific init code, e.g. rgxinit.c
	 */
	PVRSRV_ERROR			(*pfnPDumpInitDevice)(struct _PVRSRV_DEVICE_NODE_ *psDeviceNode);
	/* device-level callback to return pdump ID associated to a memory context */
	IMG_UINT32				(*pfnMMUGetContextID)(IMG_HANDLE hDevMemContext);
#endif
} PVRSRV_DEVICE_NODE;

PVRSRV_ERROR IMG_CALLCONV PVRSRVFinaliseSystem(IMG_BOOL bInitSuccesful,
														IMG_UINT32 ui32ClientBuildOptions);

PVRSRV_ERROR IMG_CALLCONV PVRSRVDevInitCompatCheck(PVRSRV_DEVICE_NODE *psDeviceNode,
														IMG_UINT32 ui32ClientBuildOptions);

#if defined(__cplusplus)
}
#endif
	
#endif /* __DEVICE_H__ */

/******************************************************************************
 End of file (device.h)
******************************************************************************/
