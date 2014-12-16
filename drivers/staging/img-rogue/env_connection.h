/*************************************************************************/ /*!
@File
@Title          Server side connection management
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Linux specific server side connection management
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(_ENV_CONNECTION_H_)
#define _ENV_CONNECTION_H_

#include <linux/list.h>

#include "handle.h"
#include "pvr_debug.h"

#if defined(SUPPORT_ION)
#include PVR_ANDROID_ION_HEADER
#include "ion_sys.h"
#include "allocmem.h"
#endif

#if defined(SUPPORT_ION)
#define ION_CLIENT_NAME_SIZE	50

typedef struct _ENV_ION_CONNECTION_DATA_
{
	IMG_CHAR azIonClientName[ION_CLIENT_NAME_SIZE];
	struct ion_device *psIonDev;
	struct ion_client *psIonClient;
	IMG_UINT32 ui32IonClientRefCount;
} ENV_ION_CONNECTION_DATA;
#endif

typedef struct _ENV_CONNECTION_DATA_
{
	struct file *psFile;

#if defined(SUPPORT_ION)
	ENV_ION_CONNECTION_DATA *psIonData;
#endif
#if defined(SUPPORT_DRM_EXT)
	IMG_VOID *pPriv;
#endif
} ENV_CONNECTION_DATA;

#if defined(SUPPORT_ION)
static inline struct ion_client *EnvDataIonClientAcquire(ENV_CONNECTION_DATA *psEnvData)
{
	PVR_ASSERT(psEnvData->psIonData != IMG_NULL);
	PVR_ASSERT(psEnvData->psIonData->psIonClient != IMG_NULL);
	PVR_ASSERT(psEnvData->psIonData->ui32IonClientRefCount > 0);
	psEnvData->psIonData->ui32IonClientRefCount++;
	return psEnvData->psIonData->psIonClient;
}

static inline void EnvDataIonClientRelease(ENV_ION_CONNECTION_DATA *psIonData)
{
	PVR_ASSERT(psIonData != IMG_NULL);
	PVR_ASSERT(psIonData->psIonClient != IMG_NULL);
	PVR_ASSERT(psIonData->ui32IonClientRefCount > 0);
	if (--psIonData->ui32IonClientRefCount == 0)
	{
		ion_client_destroy(psIonData->psIonClient);
		IonDevRelease(psIonData->psIonDev);
		OSFreeMem(psIonData);
		psIonData = IMG_NULL;
	}
}
#endif /* defined(SUPPORT_ION) */

#endif /* !defined(_ENV_CONNECTION_H_) */
