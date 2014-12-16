/*************************************************************************/ /*!
@File
@Title          Common bridge header for syncsexport
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for syncsexport
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_SYNCSEXPORT_BRIDGE_H
#define COMMON_SYNCSEXPORT_BRIDGE_H



#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_SYNCSEXPORT_CMD_FIRST			(PVRSRV_BRIDGE_SYNCSEXPORT_START)
#define PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREEXPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_SYNCSEXPORT_CMD_FIRST+0)
#define PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREUNEXPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_SYNCSEXPORT_CMD_FIRST+1)
#define PVRSRV_BRIDGE_SYNCSEXPORT_SYNCPRIMSERVERSECUREIMPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_SYNCSEXPORT_CMD_FIRST+2)
#define PVRSRV_BRIDGE_SYNCSEXPORT_CMD_LAST			(PVRSRV_BRIDGE_SYNCSEXPORT_CMD_FIRST+2)


/*******************************************
            SyncPrimServerSecureExport          
 *******************************************/

/* Bridge in structure for SyncPrimServerSecureExport */
typedef struct PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREEXPORT_TAG
{
	IMG_HANDLE hSyncHandle;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREEXPORT;


/* Bridge out structure for SyncPrimServerSecureExport */
typedef struct PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREEXPORT_TAG
{
	IMG_SECURE_TYPE Export;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREEXPORT;

/*******************************************
            SyncPrimServerSecureUnexport          
 *******************************************/

/* Bridge in structure for SyncPrimServerSecureUnexport */
typedef struct PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREUNEXPORT_TAG
{
	IMG_HANDLE hExport;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREUNEXPORT;


/* Bridge out structure for SyncPrimServerSecureUnexport */
typedef struct PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREUNEXPORT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREUNEXPORT;

/*******************************************
            SyncPrimServerSecureImport          
 *******************************************/

/* Bridge in structure for SyncPrimServerSecureImport */
typedef struct PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREIMPORT_TAG
{
	IMG_SECURE_TYPE Export;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_SYNCPRIMSERVERSECUREIMPORT;


/* Bridge out structure for SyncPrimServerSecureImport */
typedef struct PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREIMPORT_TAG
{
	IMG_HANDLE hSyncHandle;
	IMG_UINT32 ui32SyncPrimVAddr;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_SYNCPRIMSERVERSECUREIMPORT;

#endif /* COMMON_SYNCSEXPORT_BRIDGE_H */
