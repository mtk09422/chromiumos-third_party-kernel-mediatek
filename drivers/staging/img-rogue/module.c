/*************************************************************************/ /*!
@File
@Title          Linux module setup
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <linux/version.h>

#if (!defined(LDM_PLATFORM) && !defined(LDM_PCI)) || \
	(defined(LDM_PLATFORM) && defined(LDM_PCI))
	#error "LDM_PLATFORM or LDM_PCI must be defined"
#endif

#if defined(SUPPORT_DRM)
#define	PVR_MOD_STATIC
#else
#define	PVR_MOD_STATIC	static
#endif

#if defined(PVR_LDM_PLATFORM_PRE_REGISTERED)
#define PVR_USE_PRE_REGISTERED_PLATFORM_DEV
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#if defined(SUPPORT_DRM)
#include <drm/drmP.h>
#endif

#if defined(LDM_PLATFORM)
#include <linux/platform_device.h>
#endif

#if defined(LDM_PCI)
#include <linux/pci.h>
#endif

#include <linux/device.h>

#include "img_defs.h"
#include "kerneldisplay.h"
#include "mm.h"
#include "allocmem.h"
#include "mmap.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "connection_server.h"
#include "handle.h"
#include "pvr_debugfs.h"
#include "pvrmodule.h"
#include "private_data.h"
#include "driverlock.h"
#include "linkage.h"
#include "power.h"
#include "env_connection.h"
#include "sysinfo.h"
#include "pvrsrv.h"
#include "process_stats.h"

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING) || defined(SUPPORT_DRM)
#include "syscommon.h"
#endif

#if defined(SUPPORT_DRM_EXT)
#include "pvr_drm_ext.h"
#endif
#if defined(SUPPORT_DRM)
#include "pvr_drm.h"
#endif
#if defined(SUPPORT_AUTH)
#include "osauth.h"
#endif

#if defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC)
#include "pvr_sync.h"
#endif

#if defined(SUPPORT_GPUTRACE_EVENTS)
#include "pvr_gputrace.h"
#endif

#if defined(SUPPORT_KERNEL_HWPERF) || defined(SUPPORT_SHARED_SLC)
#include "rgxapi_km.h"
#endif

#if defined(SUPPORT_KERNEL_SRVINIT)
#include "srvinit.h"
#endif
/*
 * DRVNAME is the name we use to register our driver.
 * DEVNAME is the name we use to register actual device nodes.
 */
#define	DRVNAME		PVR_LDM_DRIVER_REGISTRATION_NAME
#define DEVNAME		PVRSRV_MODNAME

#if defined(SUPPORT_DRM)
#define PRIVATE_DATA(pFile) (PVR_DRM_FILE_FROM_FILE(pFile)->driver_priv)
#else
#define PRIVATE_DATA(pFile) ((pFile)->private_data)
#endif

/*
 * This is all module configuration stuff required by the linux kernel.
 */
MODULE_SUPPORTED_DEVICE(DEVNAME);

#if defined(PVRSRV_NEED_PVR_DPF)
#include <linux/moduleparam.h>
extern IMG_UINT32 gPVRDebugLevel;
module_param(gPVRDebugLevel, uint, 0644);
MODULE_PARM_DESC(gPVRDebugLevel, "Sets the level of debug output (default 0x7)");
#endif /* defined(PVRSRV_NEED_PVR_DPF) */

/*
 * Newer kernels no longer support __devinitdata, __devinit, __devexit, or
 * __devexit_p.
 */
#if !defined(__devinitdata)
#define __devinitdata
#endif
#if !defined(__devinit)
#define __devinit
#endif
#if !defined(__devexit)
#define __devexit
#endif
#if !defined(__devexit_p)
#define __devexit_p(x) (&(x))
#endif

#if defined(SUPPORT_DISPLAY_CLASS)
/* Display class interface */
EXPORT_SYMBOL(DCRegisterDevice);
EXPORT_SYMBOL(DCUnregisterDevice);
EXPORT_SYMBOL(DCDisplayConfigurationRetired);
EXPORT_SYMBOL(DCDisplayHasPendingCommand);
EXPORT_SYMBOL(DCImportBufferAcquire);
EXPORT_SYMBOL(DCImportBufferRelease);
#endif

/* Physmem interface (required by LMA DC drivers) */
EXPORT_SYMBOL(PhysHeapAcquire);
EXPORT_SYMBOL(PhysHeapRelease);
EXPORT_SYMBOL(PhysHeapGetType);
EXPORT_SYMBOL(PhysHeapGetAddress);
EXPORT_SYMBOL(PhysHeapGetSize);
EXPORT_SYMBOL(PhysHeapCpuPAddrToDevPAddr);

/* System interface (required by DC drivers) */
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING) && !defined(SUPPORT_DRM)
EXPORT_SYMBOL(SysInstallDeviceLISR);
EXPORT_SYMBOL(SysUninstallDeviceLISR);
#endif

EXPORT_SYMBOL(PVRSRVCheckStatus);
EXPORT_SYMBOL(PVRSRVGetErrorStringKM);

#if defined(SUPPORT_KERNEL_HWPERF)
EXPORT_SYMBOL(RGXHWPerfConnect);
EXPORT_SYMBOL(RGXHWPerfDisconnect);
EXPORT_SYMBOL(RGXHWPerfControl);
EXPORT_SYMBOL(RGXHWPerfConfigureAndEnableCounters);
EXPORT_SYMBOL(RGXHWPerfDisableCounters);
EXPORT_SYMBOL(RGXHWPerfAcquireData);
EXPORT_SYMBOL(RGXHWPerfReleaseData);
#endif

#if defined(SUPPORT_SHARED_SLC)
EXPORT_SYMBOL(RGXInitSLC);
#endif

#if !defined(SUPPORT_DRM)
/*
 * Device class used for /sys entries (and udev device node creation)
 */
static struct class *psPvrClass;

/*
 * This is the major number we use for all nodes in /dev.
 */
static int AssignedMajorNumber;

/*
 * These are the operations that will be associated with the device node
 * we create.
 *
 * With gcc -W, specifying only the non-null members produces "missing
 * initializer" warnings.
*/
static int PVRSRVOpen(struct inode* pInode, struct file* pFile);
static int PVRSRVRelease(struct inode* pInode, struct file* pFile);

static struct file_operations pvrsrv_fops =
{
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= PVRSRV_BridgeDispatchKM,
#if defined(CONFIG_COMPAT)
	.compat_ioctl	= PVRSRV_BridgeCompatDispatchKM,
#endif
	.open		= PVRSRVOpen,
	.release	= PVRSRVRelease,
	.mmap		= MMapPMR,
};
#endif	/* !defined(SUPPORT_DRM) */

struct mutex gPVRSRVLock;

#if defined(LDM_PLATFORM)
#define	LDM_DEV	struct platform_device
#define	LDM_DRV	struct platform_driver
#define TO_LDM_DEV(d) to_platform_device(d)
#endif /*LDM_PLATFORM */

#if defined(LDM_PCI)
#define	LDM_DEV	struct pci_dev
#define	LDM_DRV	struct pci_driver
#define TO_LDM_DEV(d) to_pci_device(d)
#endif /* LDM_PCI */

#if defined(LDM_PLATFORM)
static int PVRSRVDriverRemove(LDM_DEV *device);
static int PVRSRVDriverProbe(LDM_DEV *device);
#endif

#if defined(LDM_PCI)
static void PVRSRVDriverRemove(LDM_DEV *device);
static int PVRSRVDriverProbe(LDM_DEV *device, const struct pci_device_id *id);
#endif

static void PVRSRVDriverShutdown(LDM_DEV *device);
static int PVRSRVDriverSuspend(struct device *device);
static int PVRSRVDriverResume(struct device *device);

#if defined(LDM_PCI)
/* This structure is used by the Linux module code */
struct pci_device_id powervr_id_table[] __devinitdata = {
	{PCI_DEVICE(SYS_RGX_DEV_VENDOR_ID, SYS_RGX_DEV_DEVICE_ID)},
#if defined (SYS_RGX_DEV1_DEVICE_ID)
	{PCI_DEVICE(SYS_RGX_DEV_VENDOR_ID, SYS_RGX_DEV1_DEVICE_ID)},
#endif
	{0}
};
#if !defined(SUPPORT_DRM_EXT)
MODULE_DEVICE_TABLE(pci, powervr_id_table);
#endif
#endif /*defined(LDM_PCI) */

#if defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
static struct platform_device_id powervr_id_table[] __devinitdata = {
	{SYS_RGX_DEV_NAME, 0},
	{}
};
#endif

static struct dev_pm_ops powervr_dev_pm_ops = {
	.suspend	= PVRSRVDriverSuspend,
	.resume		= PVRSRVDriverResume,
};

#if defined(LDM_PLATFORM)
static const struct of_device_id mt_powervr_of_match[] = {
        { .compatible = "mediatek,mt8135-gpu", },
        {},
};

MODULE_DEVICE_TABLE(of,mt_powervr_of_match);
#endif

static LDM_DRV powervr_driver = {
#if defined(LDM_PLATFORM)
	.driver = {
		.name	= DRVNAME,
		.of_match_table = of_match_ptr(mt_powervr_of_match),		
		.pm	= &powervr_dev_pm_ops,
	},
#endif
#if defined(LDM_PCI)
	.name		= DRVNAME,
	.driver.pm	= &powervr_dev_pm_ops,
#endif
#if defined(LDM_PCI) || defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
	.id_table	= powervr_id_table,
#endif
	.probe		= PVRSRVDriverProbe,
#if defined(LDM_PLATFORM)
	.remove		= PVRSRVDriverRemove,
#endif
#if defined(LDM_PCI)
	.remove		= __devexit_p(PVRSRVDriverRemove),
#endif
	.shutdown	= PVRSRVDriverShutdown,
};

#if defined(SUPPORT_DRM_EXT)
extern LDM_DEV *gpsPVRLDMDev;
#else
LDM_DEV *gpsPVRLDMDev;
#endif

#if defined(LDM_PLATFORM)
#if defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
static void PVRSRVDeviceRelease(struct device unref__ *pDevice)
{
}

static struct platform_device powervr_device =
{
	.name			= DEVNAME,
	.id			= -1,
	.dev 			= {
		.release	= PVRSRVDeviceRelease
	}
};
#else
static struct platform_device_info powervr_device_info =
{
	.name			= DEVNAME,
	.id			= -1,
	.dma_mask		= DMA_BIT_MASK(32),
};
#endif	/* (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)) */
#endif	/* defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV) */
#endif	/* defined(LDM_PLATFORM) */

static IMG_BOOL bCalledSysInit = IMG_FALSE;
static IMG_BOOL	bDriverProbeSucceeded = IMG_FALSE;

/*!
******************************************************************************

 @Function		PVRSRVSystemInit

 @Description

 Wrapper for PVRSRVInit.

 @input pDevice - the device for which a probe is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
#if defined(SUPPORT_DRM)
int PVRSRVSystemInit(struct drm_device *pDevice)
#else
static int PVRSRVSystemInit(LDM_DEV *pDevice)
#endif
{
	PVR_TRACE(("PVRSRVSystemInit (pDevice=%p)", pDevice));

//	ssleep(30);

#if defined(SUPPORT_DRM)
	/* PVRSRVInit is only designed to be called once */
	if (bCalledSysInit == IMG_FALSE)
	{
#if defined(LDM_PLATFORM)
		gpsPVRLDMDev = pDevice->platformdev;
#elif defined(LDM_PCI)
		gpsPVRLDMDev = pDevice->pdev;
#else
#error Only platform and pci devices are supported
#endif

#else /* SUPPORT_DRM */
	{
		gpsPVRLDMDev = pDevice;
#endif

		bCalledSysInit = IMG_TRUE;

		if (PVRSRVInit(pDevice) != PVRSRV_OK)
		{
			return -ENODEV;
		}
	}

	return 0;
}

/*!
******************************************************************************

 @Function		PVRSRVSystemDeInit

 @Description

 Wrapper for PVRSRVDeInit.

 @input none
 @Return nothing.

*****************************************************************************/
PVR_MOD_STATIC void PVRSRVSystemDeInit(void)
{
	PVR_TRACE(("PVRSRVSystemDeInit"));

	PVRSRVDeInit();

#if !defined(LDM_PLATFORM) || (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
	gpsPVRLDMDev = IMG_NULL;
#endif
}

/*!
******************************************************************************

 @Function		PVRSRVDriverProbe

 @Description

 See whether a given device is really one we can drive.

 @input pDevice - the device for which a probe is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
#if defined(LDM_PLATFORM)
extern int MTKMFGGetClocks(LDM_DEV* pDevice);
static int PVRSRVDriverProbe(LDM_DEV *pDevice)
#endif
#if defined(LDM_PCI)
static int __devinit PVRSRVDriverProbe(LDM_DEV *pDevice, const struct pci_device_id *pID)
#endif
{
	int result = 0;

	PVR_TRACE(("PVRSRVDriverProbe (pDevice=%p)", pDevice));

#if defined(SUPPORT_DRM)
#if defined(LDM_PLATFORM) && !defined(SUPPORT_DRM_EXT)
	PVR_TRACE(("PVRSRVDriverProbe (pDevice=%p),name %s DEVNAME %s", pDevice,pDevice->name, DEVNAME));
	if (OSStringCompare(pDevice->name,DEVNAME) != 0)
	{
		result = MTKMFGGetClocks(pDevice);
	}
	result = drm_platform_init(&sPVRDRMDriver, pDevice);
#endif
#if defined(LDM_PCI) && !defined(SUPPORT_DRM_EXT)
	result = drm_get_pci_dev(pDevice, pID, &sPVRDRMDriver);
#endif
#else	/* defined(SUPPORT_DRM) */
	result = PVRSRVSystemInit(pDevice);
#endif	/* defined(SUPPORT_DRM) */
	bDriverProbeSucceeded = (result == 0);
	return result;
}


/*!
******************************************************************************

 @Function		PVRSRVDriverRemove

 @Description

 This call is the opposite of the probe call; it is called when the device is
 being removed from the driver's control.

 @input pDevice - the device for which driver detachment is happening

 @Return 0, or no return value at all, depending on the device type.

*****************************************************************************/
#if defined (LDM_PLATFORM)
static int PVRSRVDriverRemove(LDM_DEV *pDevice)
#endif
#if defined(LDM_PCI)
static void __devexit PVRSRVDriverRemove(LDM_DEV *pDevice)
#endif
{
	PVR_TRACE(("PVRSRVDriverRemove (pDevice=%p)", pDevice));

#if defined(SUPPORT_DRM) && !defined(SUPPORT_DRM_EXT)
#if defined(LDM_PLATFORM)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
	drm_platform_exit(&sPVRDRMDriver, pDevice);
#else
	drm_put_dev(platform_get_drvdata(pDevice));
#endif
#endif	/* defined(LDM_PLATFORM) */
#if defined(LDM_PCI)
	drm_put_dev(pci_get_drvdata(pDevice));
#endif
#else	/* defined(SUPPORT_DRM) */
	PVRSRVSystemDeInit();
#endif	/* defined(SUPPORT_DRM) */
#if defined(LDM_PLATFORM)
	return 0;
#endif
}

static struct mutex gsPMMutex;
static IMG_BOOL bDriverIsSuspended;
static IMG_BOOL bDriverIsShutdown;

/*!
******************************************************************************

 @Function		PVRSRVDriverShutdown

 @Description

 Suspend device operation for system shutdown.  This is called as part of the
 system halt/reboot process.  The driver is put into a quiescent state by 
 setting the power state to D3.

 @input pDevice - the device for which shutdown is requested

 @Return nothing

*****************************************************************************/
static void PVRSRVDriverShutdown(LDM_DEV *pDevice)
{
	PVR_TRACE(("PVRSRVDriverShutdown (pDevice=%p)", pDevice));

	mutex_lock(&gsPMMutex);

	if (!bDriverIsShutdown && !bDriverIsSuspended)
	{
		/*
		 * Take the bridge mutex, and never release it, to stop
		 * processes trying to use the driver after it has been
		 * shutdown.
		 */
		OSAcquireBridgeLock();

		(void) PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_OFF, IMG_TRUE);
	}

	bDriverIsShutdown = IMG_TRUE;

	/* The bridge mutex is held on exit */
	mutex_unlock(&gsPMMutex);
}

/*!
******************************************************************************

 @Function		PVRSRVDriverSuspend

 @Description

 Suspend device operation.

 @input pDevice - the device for which resume is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
static int PVRSRVDriverSuspend(struct device *pDevice)
{
	int res = 0;

	PVR_TRACE(( "PVRSRVDriverSuspend (pDevice=%p)", pDevice));

	mutex_lock(&gsPMMutex);

	if (!bDriverIsSuspended && !bDriverIsShutdown)
	{
		OSAcquireBridgeLock();

		if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_OFF, IMG_TRUE) == PVRSRV_OK)
		{
			/* The bridge mutex will be held until we resume */
			bDriverIsSuspended = IMG_TRUE;
		}
		else
		{
			OSReleaseBridgeLock();
			res = -EINVAL;
		}
	}

	mutex_unlock(&gsPMMutex);

	return res;
}


/*!
******************************************************************************

 @Function		PVRSRVDriverResume

 @Description

 Resume device operation.

 @input pDevice - the device for which resume is requested

 @Return 0 for success or <0 for an error.

*****************************************************************************/
static int PVRSRVDriverResume(struct device *pDevice)
{
	int res = 0;

	PVR_TRACE(("PVRSRVDriverResume (pDevice=%p)", pDevice));

	mutex_lock(&gsPMMutex);

	if (bDriverIsSuspended && !bDriverIsShutdown)
	{
		if (PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_ON, IMG_TRUE) == PVRSRV_OK)
		{
			bDriverIsSuspended = IMG_FALSE;
			OSReleaseBridgeLock();
		}
		else
		{
			/* The bridge mutex is not released on failure */
			res = -EINVAL;
		}
	}

	mutex_unlock(&gsPMMutex);

	return res;
}

/*!
******************************************************************************

 @Function		PVRSRVOpen

 @Description

 Open the PVR services node.

 @input pInode - the inode for the file being openeded.
 @input dev    - the DRM device corresponding to this driver.

 @input pFile - the file handle data for the actual file being opened

 @Return 0 for success or <0 for an error.

*****************************************************************************/
#if defined(SUPPORT_DRM)
int PVRSRVOpen(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
#else
static int PVRSRVOpen(struct inode unref__ *pInode, struct file *pFile)
#endif
{
#if defined(SUPPORT_DRM)
	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);
#endif
	void *pvConnectionData;
	PVRSRV_ERROR eError;

	if (!try_module_get(THIS_MODULE))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to get module"));
		return -ENOENT;
	}

	OSAcquireBridgeLock();

	/*
	 * Here we pass the file pointer which will passed through to our
	 * OSConnectionPrivateDataInit function where we can save it so
	 * we can back reference the file structure from it's connection
	 */
	eError = PVRSRVConnectionConnect(&pvConnectionData, (IMG_PVOID) pFile);
	if (eError != PVRSRV_OK)
	{
		OSReleaseBridgeLock();
		module_put(THIS_MODULE);

		return -ENOMEM;
	}

	PRIVATE_DATA(pFile) = pvConnectionData;
	OSReleaseBridgeLock();

	return 0;
}


/*!
******************************************************************************

 @Function		PVRSRVRelease

 @Description

 Release access the PVR services node - called when a file is closed, whether
 at exit or using close(2) system call.

 @input pInode - the inode for the file being released
 @input pvPrivData - driver private data

 @input pFile - the file handle data for the actual file being released

 @Return 0 for success or <0 for an error.

*****************************************************************************/
#if defined(SUPPORT_DRM)
void PVRSRVRelease(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
#else
static int PVRSRVRelease(struct inode unref__ *pInode, struct file *pFile)
#endif
{
#if defined(SUPPORT_DRM)
	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);
#endif
	void *pvConnectionData;

	OSAcquireBridgeLock();

	pvConnectionData = PRIVATE_DATA(pFile);
	if (pvConnectionData)
	{
		PVRSRVConnectionDisconnect(pvConnectionData);
		PRIVATE_DATA(pFile) = NULL;
	}

	OSReleaseBridgeLock();
	module_put(THIS_MODULE);

#if !defined(SUPPORT_DRM)
	return 0;
#endif
}

CONNECTION_DATA *LinuxConnectionFromFile(struct file *pFile)
{
	return PRIVATE_DATA(pFile);
}

struct file *LinuxFileFromConnection(CONNECTION_DATA *psConnection)
{
	ENV_CONNECTION_DATA *psEnvConnection;

	psEnvConnection = PVRSRVConnectionPrivateData(psConnection);
	PVR_ASSERT(psEnvConnection != NULL);
	
	return psEnvConnection->psFile;
}

#if defined(SUPPORT_AUTH)
PVRSRV_ERROR OSCheckAuthentication(CONNECTION_DATA *psConnection, IMG_UINT32 ui32Level)
{
	if (ui32Level != 0)
	{
		ENV_CONNECTION_DATA *psEnvConnection;

		psEnvConnection = PVRSRVConnectionPrivateData(psConnection);
		if (psEnvConnection == IMG_NULL)
		{
			return PVRSRV_ERROR_RESOURCE_UNAVAILABLE;
		}

		if (!PVR_DRM_FILE_FROM_FILE(psEnvConnection->psFile)->authenticated)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: PVR Services Connection not authenticated", __FUNCTION__));
			return PVRSRV_ERROR_NOT_AUTHENTICATED;
		}
	}

	return PVRSRV_OK;
}
#endif /* defined(SUPPORT_AUTH) */

/*!
******************************************************************************

 @Function		PVRCore_Init

 @Description

 Insert the driver into the kernel.

 Readable and/or writable debugfs entries under /sys/kernel/debug/pvr are
 created with PVRDebugFSCreateEntry().  These can be read at runtime to get
 information about the device (eg. 'cat /sys/kernel/debug/pvr/nodes')

 __init places the function in a special memory section that the kernel frees
 once the function has been run.  Refer also to module_init() macro call below.

 @input none

 @Return none

*****************************************************************************/

#if defined(SUPPORT_KERNEL_SRVINIT)
static void PVRCore_Cleanup(void);
#endif

/* FIXME: This is declared here to save creating a new header which
          should be removed soon anyway as bridge gen should be providing
          this interface */
PVRSRV_ERROR LinuxBridgeInit(void);
void LinuxBridgeDeInit(void);

#if defined(SUPPORT_DRM_EXT)
int PVRCore_Init(void)
#else
static int __init PVRCore_Init(void)
#endif
{
	int error;
#if defined(PVRSRV_ENABLE_PROCESS_STATS) || defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC) || defined(SUPPORT_KERNEL_SRVINIT)
	PVRSRV_ERROR eError;
#endif
#if !defined(SUPPORT_DRM)
	struct device *psDev;
#endif

	/*
	 * Must come before attempting to print anything via Services.
	 * For DRM, the initialisation will already have been done.
	 */
	PVRDPFInit();

	PVR_TRACE(("PVRCore_Init"));

#if defined(SUPPORT_DRM)
#if defined(PDUMP)
	error = dbgdrv_init();
	if (error != 0)
	{
		return error;
	}
#endif
#endif

	mutex_init(&gsPMMutex);

	mutex_init(&gPVRSRVLock);

	error = PVRDebugFSInit();
	if (error != 0)
	{
		goto dbgdrv_cleanup;
	}

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	eError = PVRSRVStatsInitialise();
	if (eError != PVRSRV_OK)
	{
		error = -ENOMEM;

		goto debugfs_deinit;
	}
#endif

	if (PVROSFuncInit() != PVRSRV_OK)
	{
		error = -ENOMEM;
		goto init_failed;
	}

	LinuxBridgeInit();

	PVRMMapInit();

#if defined(LDM_PLATFORM)
	if ((error = platform_driver_register(&powervr_driver)) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register platform driver (%d)", error));

		goto init_failed;
	}

#if defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
	error = platform_device_register(&powervr_device);
#else
	gpsPVRLDMDev = platform_device_register_full(&powervr_device_info);
	error = IS_ERR(gpsPVRLDMDev) ? PTR_ERR(gpsPVRLDMDev) : 0;
#endif
	if (error != 0)
	{
		gpsPVRLDMDev = NULL;
		platform_driver_unregister(&powervr_driver);

		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register platform device (%d)", error));

		goto init_failed;
	}
#endif	/* defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV) */
#endif	/* defined(LDM_PLATFORM) */ 

#if defined(LDM_PCI) && !defined(SUPPORT_DRM_EXT)
#if defined(SUPPORT_DRM)
	error = drm_pci_init(&sPVRDRMDriver, &powervr_driver);
#else
	error = pci_register_driver(&powervr_driver);
#endif
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register PCI driver (%d)", error));

		goto init_failed;
	}
#endif /* LDM_PCI */

	/* Check that the driver probe function was called */
#if defined(LDM_PCI) && defined(SUPPORT_DRM_EXT)
	if (!bDriverProbeSucceeded)
	{
		error = PVRSRVInit(gpsPVRLDMDev);
		if (error != 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVSystemInit: unable to init PVR service (%d)", error));

			goto init_failed;
		}
		bDriverProbeSucceeded = IMG_TRUE;
	}
#endif	/* defined(SUPPORT_DRM) */

	if (!bDriverProbeSucceeded)
	{
		PVR_TRACE(("PVRCore_Init: PVRSRVDriverProbe has not been called or did not succeed - check that hardware is detected"));
		goto init_failed;
	}

#if !defined(SUPPORT_DRM)
	AssignedMajorNumber = register_chrdev(0, DEVNAME, &pvrsrv_fops);

	if (AssignedMajorNumber <= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to get major number"));

		error = -EBUSY;
		goto sys_deinit;
	}

	PVR_TRACE(("PVRCore_Init: major device %d", AssignedMajorNumber));

	/*
	 * This code facilitates automatic device node creation on platforms
	 * with udev (or similar).
	 */
	psPvrClass = class_create(THIS_MODULE, "pvr");

	if (IS_ERR(psPvrClass))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to create class (%ld)", PTR_ERR(psPvrClass)));
		error = -EBUSY;
		goto unregister_device;
	}

	psDev = device_create(psPvrClass, NULL, MKDEV(AssignedMajorNumber, 0),
				  NULL, DEVNAME);
	if (IS_ERR(psDev))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to create device (%ld)", PTR_ERR(psDev)));
		error = -EBUSY;
		goto destroy_class;
	}
#endif /* !defined(SUPPORT_DRM) */

#if defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC)
	eError = pvr_sync_init();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to create sync (%d)", eError));
		error = -EBUSY;
#if !defined(SUPPORT_DRM)
		goto destroy_class;
#else
		goto init_failed;
#endif

	}
#endif

	error = PVRDebugCreateDebugFSEntries();
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "PVRCore_Init: failed to create default debugfs entries (%d)", error));
	}

#if defined(SUPPORT_GPUTRACE_EVENTS)
	error = PVRGpuTraceInit();
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "PVRCore_Init: failed to initialise PVR GPU Tracing (%d)", error));
	}
#endif

#if defined(SUPPORT_KERNEL_SRVINIT)
	eError = SrvInit();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: SrvInit failed (%d)", eError));
		PVRCore_Cleanup();
		return -ENODEV;
	}
#endif
	return 0;

#if !defined(SUPPORT_DRM)
destroy_class:
	class_destroy(psPvrClass);
unregister_device:
	unregister_chrdev((IMG_UINT)AssignedMajorNumber, DEVNAME);
sys_deinit:
#if defined(LDM_PCI)
#if defined(SUPPORT_DRM) && !defined(SUPPORT_DRM_EXT)
	drm_pci_exit(&sPVRDRMDriver, &powervr_driver);
#else
	pci_unregister_driver(&powervr_driver);
#endif
#endif

#if defined (LDM_PLATFORM)
#if defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
	platform_device_unregister(&powervr_device);
#else
	PVR_ASSERT(gpsPVRLDMDev != NULL);
	platform_device_unregister(gpsPVRLDMDev);
#endif	/* (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)) */
#endif	/* defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV) */
	platform_driver_unregister(&powervr_driver);
#endif	/* defined(LDM_MODULE) */
#endif	/* !defined(SUPPORT_DRM) */

init_failed:
#if defined(LDM_PCI) && defined(SUPPORT_DRM_EXT)
	PVRSRVDeInit();
#endif
	PVRMMapCleanup();
	LinuxBridgeDeInit();
	PVROSFuncDeInit();
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsDestroy();
debugfs_deinit:
#endif
	PVRDebugFSDeInit();
dbgdrv_cleanup:
#if defined(SUPPORT_DRM)
#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
#endif
	return error;

} /*PVRCore_Init*/


/*!
*****************************************************************************

 @Function		PVRCore_Cleanup

 @Description	

 Remove the driver from the kernel.

 There's no way we can get out of being unloaded other than panicking; we
 just do everything and plough on regardless of error.

 __exit places the function in a special memory section that the kernel frees
 once the function has been run.  Refer also to module_exit() macro call below.

 @input none

 @Return none

*****************************************************************************/
#if defined(SUPPORT_KERNEL_SRVINIT)
static void PVRCore_Cleanup(void)
#else
#if defined(SUPPORT_DRM_EXT)
void PVRCore_Cleanup(void)
#else
static void __exit PVRCore_Cleanup(void)
#endif
#endif
{
	PVR_TRACE(("PVRCore_Cleanup"));

#if defined(SUPPORT_GPUTRACE_EVENTS)
	PVRGpuTraceDeInit();
#endif

	PVRDebugRemoveDebugFSEntries();

#if defined(PVR_ANDROID_NATIVE_WINDOW_HAS_SYNC)
	pvr_sync_deinit();
#endif

#if !defined(SUPPORT_DRM)
	device_destroy(psPvrClass, MKDEV(AssignedMajorNumber, 0));
	class_destroy(psPvrClass);

	unregister_chrdev((IMG_UINT)AssignedMajorNumber, DEVNAME);
#endif

#if defined(LDM_PCI)
#if defined(SUPPORT_DRM) && !defined(SUPPORT_DRM_EXT)
	drm_pci_exit(&sPVRDRMDriver, &powervr_driver);
#else
	pci_unregister_driver(&powervr_driver);
#endif
#endif	/* defined(LDM_PCI) */

#if defined (LDM_PLATFORM)
#if defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
	platform_device_unregister(&powervr_device);
#else
	PVR_ASSERT(gpsPVRLDMDev != NULL);
	platform_device_unregister(gpsPVRLDMDev);
#endif	/* (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)) */
#endif	/* defined(MODULE) && !defined(PVR_USE_PRE_REGISTERED_PLATFORM_DEV) */
	platform_driver_unregister(&powervr_driver);
#endif	/* defined (LDM_PLATFORM) */

	PVRMMapCleanup();

	LinuxBridgeDeInit();

	PVROSFuncDeInit();

#if defined(PVRSRV_ENABLE_PROCESS_STATS)
	PVRSRVStatsDestroy();
#endif
	PVRDebugFSDeInit();

#if defined(SUPPORT_DRM)
#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
#endif
	PVR_TRACE(("PVRCore_Cleanup: unloading"));
}

/*
 * These macro calls define the initialisation and removal functions of the
 * driver.  Although they are prefixed `module_', they apply when compiling
 * statically as well; in both cases they define the function the kernel will
 * run to start/stop the driver.
*/
#if !defined(SUPPORT_DRM_EXT)
module_init(PVRCore_Init);
module_exit(PVRCore_Cleanup);
#endif
