/*************************************************************************/ /*!
@File
@Title          Common bridge header for pdump
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for pdump
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_PDUMP_BRIDGE_H
#define COMMON_PDUMP_BRIDGE_H

#include "devicemem_typedefs.h"
#include "pdumpdefs.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_PDUMP_CMD_FIRST			(PVRSRV_BRIDGE_PDUMP_START)
#define PVRSRV_BRIDGE_PDUMP_DEVMEMPDUMPBITMAP			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMP_CMD_FIRST+0)
#define PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPCOMMENT			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMP_CMD_FIRST+1)
#define PVRSRV_BRIDGE_PDUMP_CMD_LAST			(PVRSRV_BRIDGE_PDUMP_CMD_FIRST+1)


/*******************************************
            DevmemPDumpBitmap          
 *******************************************/

/* Bridge in structure for DevmemPDumpBitmap */
typedef struct PVRSRV_BRIDGE_IN_DEVMEMPDUMPBITMAP_TAG
{
	IMG_HANDLE hDeviceNode;
	IMG_CHAR * puiFileName;
	IMG_UINT32 ui32FileOffset;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32StrideInBytes;
	IMG_DEV_VIRTADDR sDevBaseAddr;
	IMG_HANDLE hDevmemCtx;
	IMG_UINT32 ui32Size;
	PDUMP_PIXEL_FORMAT ePixelFormat;
	IMG_UINT32 ui32AddrMode;
	IMG_UINT32 ui32PDumpFlags;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_DEVMEMPDUMPBITMAP;


/* Bridge out structure for DevmemPDumpBitmap */
typedef struct PVRSRV_BRIDGE_OUT_DEVMEMPDUMPBITMAP_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_DEVMEMPDUMPBITMAP;

/*******************************************
            PVRSRVPDumpComment          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpComment */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT_TAG
{
	IMG_CHAR * puiComment;
	IMG_UINT32 ui32Flags;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT;


/* Bridge out structure for PVRSRVPDumpComment */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT;

#endif /* COMMON_PDUMP_BRIDGE_H */
