/**************************************************************************/ /*!
@File
@Title          Implementation Callbacks for Handle Manager API
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the handle manager API. This file is for declarations 
                and definitions that are private/internal to the handle manager 
                API but need to be shared between the generic handle manager 
                code and the various handle manager backends, i.e. the code that 
                implements the various callbacks.
@License        Strictly Confidential.
*/ /***************************************************************************/

#if !defined(__HANDLE_IMPL_H__)
#define __HANDLE_IMPL_H__

#include "img_types.h"
#include "pvrsrv_error.h"

typedef struct _HANDLE_IMPL_BASE_ HANDLE_IMPL_BASE;

typedef PVRSRV_ERROR (*PFN_HANDLE_ITER)(IMG_HANDLE hHandle, void *pvData);

typedef struct _HANDLE_IMPL_FUNCTAB_
{
	/* Acquire a new handle which is associated with the given data */
	PVRSRV_ERROR (*pfnAcquireHandle)(HANDLE_IMPL_BASE *psHandleBase, IMG_HANDLE *phHandle, void *pvData);

	/* Release the given handle (optionally returning the data associated with it) */
	PVRSRV_ERROR (*pfnReleaseHandle)(HANDLE_IMPL_BASE *psHandleBase, IMG_HANDLE hHandle, void **ppvData);

	/* Get the data associated with the given handle */
	PVRSRV_ERROR (*pfnGetHandleData)(HANDLE_IMPL_BASE *psHandleBase, IMG_HANDLE hHandle, void **ppvData);

	/* Set the data associated with the given handle */
	PVRSRV_ERROR (*pfnSetHandleData)(HANDLE_IMPL_BASE *psHandleBase, IMG_HANDLE hHandle, void *pvData);

	PVRSRV_ERROR (*pfnIterateOverHandles)(HANDLE_IMPL_BASE *psHandleBase, PFN_HANDLE_ITER pfnHandleIter, void *pvHandleIterData);

	/* Enable handle purging on the given handle base */
	PVRSRV_ERROR (*pfnEnableHandlePurging)(HANDLE_IMPL_BASE *psHandleBase);

	/* Purge handles on the given handle base */
	PVRSRV_ERROR (*pfnPurgeHandles)(HANDLE_IMPL_BASE *psHandleBase);

	/* Create handle base */
	PVRSRV_ERROR (*pfnCreateHandleBase)(HANDLE_IMPL_BASE **psHandleBase);

	/* Destroy handle base */
	PVRSRV_ERROR (*pfnDestroyHandleBase)(HANDLE_IMPL_BASE *psHandleBase);
} HANDLE_IMPL_FUNCTAB;

PVRSRV_ERROR PVRSRVHandleGetFuncTable(HANDLE_IMPL_FUNCTAB const **ppsFuncs);

#endif /* !defined(__HANDLE_IMPL_H__) */
