/*************************************************************************/ /*!
@File
@Title          Common bridge header for cmm
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for cmm
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_CMM_BRIDGE_H
#define COMMON_CMM_BRIDGE_H

#include "devicemem_typedefs.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_CMM_CMD_FIRST			(PVRSRV_BRIDGE_CMM_START)
#define PVRSRV_BRIDGE_CMM_DEVMEMINTCTXEXPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_CMM_CMD_FIRST+0)
#define PVRSRV_BRIDGE_CMM_DEVMEMINTCTXUNEXPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_CMM_CMD_FIRST+1)
#define PVRSRV_BRIDGE_CMM_DEVMEMINTCTXIMPORT			PVRSRV_IOWR(PVRSRV_BRIDGE_CMM_CMD_FIRST+2)
#define PVRSRV_BRIDGE_CMM_CMD_LAST			(PVRSRV_BRIDGE_CMM_CMD_FIRST+2)


/*******************************************
            DevmemIntCtxExport          
 *******************************************/

/* Bridge in structure for DevmemIntCtxExport */
typedef struct PVRSRV_BRIDGE_IN_DEVMEMINTCTXEXPORT_TAG
{
	IMG_HANDLE hDevMemServerContext;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_DEVMEMINTCTXEXPORT;


/* Bridge out structure for DevmemIntCtxExport */
typedef struct PVRSRV_BRIDGE_OUT_DEVMEMINTCTXEXPORT_TAG
{
	IMG_HANDLE hDevMemIntCtxExport;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_DEVMEMINTCTXEXPORT;

/*******************************************
            DevmemIntCtxUnexport          
 *******************************************/

/* Bridge in structure for DevmemIntCtxUnexport */
typedef struct PVRSRV_BRIDGE_IN_DEVMEMINTCTXUNEXPORT_TAG
{
	IMG_HANDLE hDevMemIntCtxExport;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_DEVMEMINTCTXUNEXPORT;


/* Bridge out structure for DevmemIntCtxUnexport */
typedef struct PVRSRV_BRIDGE_OUT_DEVMEMINTCTXUNEXPORT_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_DEVMEMINTCTXUNEXPORT;

/*******************************************
            DevmemIntCtxImport          
 *******************************************/

/* Bridge in structure for DevmemIntCtxImport */
typedef struct PVRSRV_BRIDGE_IN_DEVMEMINTCTXIMPORT_TAG
{
	IMG_HANDLE hDevMemIntCtxExport;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_DEVMEMINTCTXIMPORT;


/* Bridge out structure for DevmemIntCtxImport */
typedef struct PVRSRV_BRIDGE_OUT_DEVMEMINTCTXIMPORT_TAG
{
	IMG_HANDLE hDevMemServerContext;
	IMG_HANDLE hPrivData;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_DEVMEMINTCTXIMPORT;

#endif /* COMMON_CMM_BRIDGE_H */
