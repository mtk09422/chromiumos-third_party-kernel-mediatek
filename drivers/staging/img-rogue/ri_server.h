/*************************************************************************/ /*!
@File			ri_server.h
@Title          Resource Information abstraction
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description	Resource Information (RI) functions
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _RI_SERVER_H_
#define _RI_SERVER_H_

#include <img_defs.h>
#include <ri_typedefs.h>
#include <pmr.h>
#include <pvrsrv_error.h>

PVRSRV_ERROR RIInitKM(IMG_VOID);
IMG_VOID RIDeInitKM(IMG_VOID);

PVRSRV_ERROR RIWritePMREntryKM(PMR *hPMR,
					   	   	   IMG_UINT32 ui32TextASize,
					   	   	   const IMG_CHAR ai8TextA[RI_MAX_TEXT_LEN+1],
					   	   	   IMG_UINT64 uiLogicalSize);

PVRSRV_ERROR RIWriteMEMDESCEntryKM(PMR *hPMR,
					   	   	   	   IMG_UINT32 ui32TextBSize,
					   	   	   	   const IMG_CHAR ai8TextB[RI_MAX_TEXT_LEN+1],
					   	   	   	   IMG_UINT64 uiOffset,
					   	   	   	   IMG_UINT64 uiSize,
					   	   	   	   IMG_BOOL bIsImport,
					   	   	   	   IMG_BOOL bIsExportable,
					   	   	   	   RI_HANDLE *phRIHandle);

PVRSRV_ERROR RIUpdateMEMDESCAddrKM(RI_HANDLE hRIHandle,
								   IMG_DEV_VIRTADDR sVAddr);

PVRSRV_ERROR RIDeletePMREntryKM(RI_HANDLE hRIHandle);
PVRSRV_ERROR RIDeleteMEMDESCEntryKM(RI_HANDLE hRIHandle);

PVRSRV_ERROR RIDeleteListKM(IMG_VOID);

PVRSRV_ERROR RIDumpListKM(PMR *hPMR);

PVRSRV_ERROR RIDumpAllKM(IMG_VOID);

PVRSRV_ERROR RIDumpProcessKM(IMG_PID pid);

IMG_BOOL RIGetListEntryKM(IMG_PID pid,
						  IMG_HANDLE **ppHandle,
						  IMG_CHAR **ppszEntryString);

#endif /* #ifndef _RI_SERVER_H _*/
