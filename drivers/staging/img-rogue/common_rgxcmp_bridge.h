/*************************************************************************/ /*!
@File
@Title          Common bridge header for rgxcmp
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for rgxcmp
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_RGXCMP_BRIDGE_H
#define COMMON_RGXCMP_BRIDGE_H

#include "rgx_bridge.h"
#include "sync_external.h"
#include "rgx_fwif_shared.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_RGXCMP_CMD_FIRST			(PVRSRV_BRIDGE_RGXCMP_START)
#define PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+0)
#define PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+1)
#define PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+2)
#define PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+3)
#define PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+4)
#define PVRSRV_BRIDGE_RGXCMP_RGXKICKSYNCCDM			PVRSRV_IOWR(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+5)
#define PVRSRV_BRIDGE_RGXCMP_CMD_LAST			(PVRSRV_BRIDGE_RGXCMP_CMD_FIRST+5)


/*******************************************
            RGXCreateComputeContext          
 *******************************************/

/* Bridge in structure for RGXCreateComputeContext */
typedef struct PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT_TAG
{
	IMG_HANDLE hDevNode;
	IMG_UINT32 ui32Priority;
	IMG_DEV_VIRTADDR sMCUFenceAddr;
	IMG_UINT32 ui32FrameworkCmdize;
	IMG_BYTE * psFrameworkCmd;
	IMG_HANDLE hPrivData;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT;


/* Bridge out structure for RGXCreateComputeContext */
typedef struct PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT_TAG
{
	IMG_HANDLE hComputeContext;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT;

/*******************************************
            RGXDestroyComputeContext          
 *******************************************/

/* Bridge in structure for RGXDestroyComputeContext */
typedef struct PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT_TAG
{
	IMG_HANDLE hComputeContext;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT;


/* Bridge out structure for RGXDestroyComputeContext */
typedef struct PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT;

/*******************************************
            RGXKickCDM          
 *******************************************/

/* Bridge in structure for RGXKickCDM */
typedef struct PVRSRV_BRIDGE_IN_RGXKICKCDM_TAG
{
	IMG_HANDLE hComputeContext;
	IMG_UINT32 ui32ClientFenceCount;
	PRGXFWIF_UFO_ADDR * psClientFenceUFOAddress;
	IMG_UINT32 * pui32ClientFenceValue;
	IMG_UINT32 ui32ClientUpdateCount;
	PRGXFWIF_UFO_ADDR * psClientUpdateUFOAddress;
	IMG_UINT32 * pui32ClientUpdateValue;
	IMG_UINT32 ui32ServerSyncCount;
	IMG_UINT32 * pui32ServerSyncFlags;
	IMG_HANDLE * phServerSyncs;
	IMG_UINT32 ui32CmdSize;
	IMG_BYTE * psDMCmd;
	IMG_BOOL bbPDumpContinuous;
	IMG_UINT32 ui32ExternalJobReference;
	IMG_UINT32 ui32InternalJobReference;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXKICKCDM;


/* Bridge out structure for RGXKickCDM */
typedef struct PVRSRV_BRIDGE_OUT_RGXKICKCDM_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXKICKCDM;

/*******************************************
            RGXFlushComputeData          
 *******************************************/

/* Bridge in structure for RGXFlushComputeData */
typedef struct PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA_TAG
{
	IMG_HANDLE hComputeContext;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA;


/* Bridge out structure for RGXFlushComputeData */
typedef struct PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA;

/*******************************************
            RGXSetComputeContextPriority          
 *******************************************/

/* Bridge in structure for RGXSetComputeContextPriority */
typedef struct PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY_TAG
{
	IMG_HANDLE hComputeContext;
	IMG_UINT32 ui32Priority;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY;


/* Bridge out structure for RGXSetComputeContextPriority */
typedef struct PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY;

/*******************************************
            RGXKickSyncCDM          
 *******************************************/

/* Bridge in structure for RGXKickSyncCDM */
typedef struct PVRSRV_BRIDGE_IN_RGXKICKSYNCCDM_TAG
{
	IMG_HANDLE hComputeContext;
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
	IMG_BOOL bbPDumpContinuous;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_RGXKICKSYNCCDM;


/* Bridge out structure for RGXKickSyncCDM */
typedef struct PVRSRV_BRIDGE_OUT_RGXKICKSYNCCDM_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_RGXKICKSYNCCDM;

#endif /* COMMON_RGXCMP_BRIDGE_H */
