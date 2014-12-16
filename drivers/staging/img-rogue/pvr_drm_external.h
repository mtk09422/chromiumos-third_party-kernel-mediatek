/*************************************************************************/ /*!
@File
@Title          Services external DRM interface header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Services DRM declarations and definitions that are visible
                internally and externally
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(_PVR_DRM_EXTERNAL_)
#define _PVR_DRM_EXTERNAL_

typedef enum _PVRSRV_GEM_SYNC_TYPE_
{
	PVRSRV_GEM_SYNC_TYPE_WRITE = 0,
	PVRSRV_GEM_SYNC_TYPE_READ_HW,
	PVRSRV_GEM_SYNC_TYPE_READ_SW,
	PVRSRV_GEM_SYNC_TYPE_READ_DISPLAY,
	PVRSRV_GEM_SYNC_TYPE_COUNT
} PVRSRV_GEM_SYNC_TYPE;

#endif /* !defined(_PVR_DRM_EXTERNAL_) */
