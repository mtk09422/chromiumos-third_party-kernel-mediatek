/**************************************************************************/ /*!
@File
@Title          Server side connection management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    API for server side connection management
@License        Strictly Confidential.
*/ /***************************************************************************/

#if !defined(_CONNECTION_SERVER_H_)
#define _CONNECTION_SERVER_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_types.h"
#include "handle.h"

/* Variable used to hold in memory the timeout for the current time slice*/
extern IMG_UINT64 gui64TimesliceLimit;
/* Counter number of handle data freed during the current time slice */
extern IMG_UINT32 gui32HandleDataFreeCounter;
/* Set the maximum time the freeing of the resources can keep the lock */
#define CONNECTION_DEFERRED_CLEANUP_TIMESLICE_NS 3000 * 1000 /* 3ms */

typedef struct _CONNECTION_DATA_
{
	PVRSRV_HANDLE_BASE		*psHandleBase;
	struct _SYNC_CONNECTION_DATA_	*psSyncConnectionData;
	struct _PDUMP_CONNECTION_DATA_	*psPDumpConnectionData;

	/* True if the process is the initialisation server. */
	IMG_BOOL			bInitProcess;

	/*
	 * OS specific data can be stored via this handle.
	 * See osconnection_server.h for a generic mechanism
	 * for initialising this field.
	 */
	IMG_HANDLE			hOsPrivateData;

	IMG_PVOID			hSecureData;

	IMG_HANDLE			hProcessStats;

	/* List navigation for deferred freeing of connection data */
	struct _CONNECTION_DATA_	**ppsThis;
	struct _CONNECTION_DATA_	*psNext;
} CONNECTION_DATA;

PVRSRV_ERROR PVRSRVConnectionConnect(IMG_PVOID *ppvPrivData, IMG_PVOID pvOSData);
void PVRSRVConnectionDisconnect(IMG_PVOID pvPrivData);

IMG_BOOL PVRSRVConnectionPurgeDeferred(IMG_UINT64 ui64TimeSlice);

PVRSRV_ERROR PVRSRVConnectionInit(void);
PVRSRV_ERROR PVRSRVConnectionDeInit(void);


#ifdef INLINE_IS_PRAGMA
#pragma inline(PVRSRVConnectionPrivateData)
#endif
static INLINE
IMG_HANDLE PVRSRVConnectionPrivateData(CONNECTION_DATA *psConnection)
{
	return (psConnection != IMG_NULL) ? psConnection->hOsPrivateData : IMG_NULL;
}

#if defined(__cplusplus)
}
#endif

#endif /* !defined(_CONNECTION_SERVER_H_) */
