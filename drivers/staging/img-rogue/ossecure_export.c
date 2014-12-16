/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <linux/cred.h>

#include "img_types.h"
#include "ossecure_export.h"
#include "private_data.h"
#include "pvr_debug.h"
#include "driverlock.h"

#if defined(SUPPORT_DRM)
#include "pvr_drm.h"
#endif

PVRSRV_ERROR OSSecureExport(CONNECTION_DATA *psConnection,
							IMG_PVOID pvData,
							IMG_SECURE_TYPE *phSecure,
							CONNECTION_DATA **ppsSecureConnection)
{
	CONNECTION_DATA *psSecureConnection;
	struct file *connection_file;
	struct file *secure_file;
	struct dentry *secure_dentry;
	struct vfsmount *secure_mnt;
	int secure_fd;
	IMG_BOOL bPmrUnlocked = IMG_FALSE;
	PVRSRV_ERROR eError;

	/* Obtain the current connections struct file */
	connection_file = LinuxFileFromConnection(psConnection);

	/* Allocate a fd number */
	secure_fd = get_unused_fd();
	if (secure_fd < 0)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e0;
	}

	/*
		Get a reference to the dentry so when close is called we don't
		drop the last reference too early and delete the file
	*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	secure_dentry = dget(connection_file->f_path.dentry);
	secure_mnt = mntget(connection_file->f_path.mnt);
#else
	secure_dentry = dget(connection_file->f_dentry);
	secure_mnt = mntget(connection_file->f_vfsmnt);
#endif

	/* PMR lock needs to be released before bridge lock to keep lock hierarchy
	 * and avoid deadlock situation.
	 * OSSecureExport() can be called from functions that are not acquiring
	 * PMR lock (e.g. by PVRSRVSyncPrimServerSecureExportKM()) so we have to
	 * check if PMR lock is locked. */
	if (PMRIsLockedByMe())
	{
		PMRUnlock();
		bPmrUnlocked = IMG_TRUE;
	}
	OSReleaseBridgeLock();

	/* Open our device (using the file information from our current connection) */
	secure_file = dentry_open(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
					  &connection_file->f_path,
#else
					  connection_file->f_dentry,
					  connection_file->f_vfsmnt,
#endif
					  connection_file->f_flags,
					  current_cred());

	OSAcquireBridgeLock();
	if (bPmrUnlocked)
		PMRLock();

	/* Bail if the open failed */
	if (IS_ERR(secure_file))
	{
		put_unused_fd(secure_fd);
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto e0;
	}

	/* Bind our struct file with it's fd number */
	fd_install(secure_fd, secure_file);

	/* Return the new services connection our secure data created */
	psSecureConnection = LinuxConnectionFromFile(secure_file);

	/* Save the private data */
	PVR_ASSERT(psSecureConnection->hSecureData == IMG_NULL);
	psSecureConnection->hSecureData = pvData;

	*phSecure = secure_fd;
	*ppsSecureConnection = psSecureConnection;
	return PVRSRV_OK;

e0:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

PVRSRV_ERROR OSSecureImport(IMG_SECURE_TYPE hSecure, IMG_PVOID *ppvData)
{
	struct file *secure_file;
	CONNECTION_DATA *psSecureConnection;
	PVRSRV_ERROR eError;

	secure_file = fget(hSecure);

	if (!secure_file)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_out;
	}

	psSecureConnection = LinuxConnectionFromFile(secure_file);
	if (psSecureConnection->hSecureData == IMG_NULL)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto err_fput;
	}

	*ppvData = psSecureConnection->hSecureData;
	fput(secure_file);
	return PVRSRV_OK;

err_fput:
	fput(secure_file);
err_out:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}
