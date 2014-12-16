/*************************************************************************/ /*!
@File
@Title          PowerVR drm driver
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Linux module setup
@License        Strictly Confidential.
*/ /**************************************************************************/

#if defined(SUPPORT_DRM)

#include <linux/version.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/ioctl.h>
#include <drm/drmP.h>
#include <drm/drm.h>

#include "allocmem.h"
#include "img_defs.h"
#include "services.h"
#include "sysinfo.h"
#include "mm.h"
#include "mmap.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "handle.h"
#include "pvr_bridge.h"
#include "pvrmodule.h"
#include "pvrversion.h"
#include "linkage.h"
#include "pvr_drm.h"
#if !defined(SUPPORT_KERNEL_SRVINIT)
#include "power.h"
#include "driverlock.h"
#endif	/* #if !defined(SUPPORT_KERNEL_SRVINIT) */
#include "private_data.h"
#include "syscommon.h"
#include "pvrsrv.h"
#include "lists.h"
#include "device.h"

#define PVR_DRM_NAME	PVR_DRM_DRIVER_NAME
#define PVR_DRM_DESC	"Imagination Technologies PVR DRM"
#define	PVR_DRM_DATE	"20110701"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0))
#define PVR_DRM_RENDER_ALLOW DRM_RENDER_ALLOW
#else
#define PVR_DRM_RENDER_ALLOW 0
#endif

DECLARE_WAIT_QUEUE_HEAD(sWaitForInit);

/* Once bInitComplete and bInitFailed are set, they stay set */
IMG_BOOL bInitComplete;
IMG_BOOL bInitFailed;

static void *PVRSRVFindRGXDevNode(PVRSRV_DEVICE_NODE *psDevNode)
{
	if (psDevNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
	{
		return psDevNode;
	}

	return NULL;
}

static int PVRSRVDRMLoad(struct drm_device *dev, unsigned long flags)
{
	struct pvr_drm_dev_priv *psDevPriv;
	int iRes = 0;
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	PVRSRV_ERROR eError;
#endif

	PVR_TRACE(("PVRSRVDRMLoad"));

#if defined(LDM_PLATFORM)
	/* The equivalent is done for PCI modesetting drivers by drm_get_pci_dev() */
	platform_set_drvdata(dev->platformdev, dev);
#endif

	psDevPriv = OSAllocZMem(sizeof *psDevPriv);
	if (psDevPriv == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate DRM device private data", __FUNCTION__));
		iRes = -ENOMEM;
		goto error_exit;
	}
	dev->dev_private = psDevPriv;

	/* Module initialisation */
	iRes = PVRSRVSystemInit(dev);
	if (iRes != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: System initialisation failed (error = %d)", __FUNCTION__, iRes));
		goto error_free_dev_priv;
	}

	psDevPriv->dev_node = List_PVRSRV_DEVICE_NODE_Any(PVRSRVGetPVRSRVData()->psDeviceNodeList,
							  PVRSRVFindRGXDevNode);
	if (psDevPriv->dev_node == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get Rogue device node", __FUNCTION__));
		iRes = -ENODEV;
		goto error_system_deinit;
	}

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	/* Acquire the system data so it can be used by our interrupt handler */
	eError = SysAcquireSystemData(psDevPriv->dev_node->psDevConfig->hSysData);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire system data", __FUNCTION__));
		iRes = -EINVAL;
		goto error_system_deinit;
	}
	psDevPriv->hSysData = psDevPriv->dev_node->psDevConfig->hSysData;

	/* This is currently the only valid configuration option for systems using system interrupt handling */
	PVR_ASSERT(psDevPriv->dev_node->psDevConfig->eIRQActiveLevel == PVRSRV_DEVICE_IRQ_ACTIVE_SYSDEFAULT);

	if (psDevPriv->dev_node->psDevConfig->bIRQIsShared)
	{
		dev->driver->driver_features |= DRIVER_IRQ_SHARED;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
	iRes = drm_irq_install(dev);
#else
#if defined(LDM_PLATFORM)
	iRes = drm_irq_install(dev, platform_get_irq(dev->platformdev, 0));
#else
	iRes =  drm_irq_install(dev, dev->pdev->irq);
#endif
#endif
	if (iRes != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to install interrupt handler (error = %d)", __FUNCTION__, iRes));
		goto error_system_data_release;
	}
#endif

#if defined(SUPPORT_DRM_DC_MODULE)
	iRes = PVRSRVDRMDisplayInit(dev);
	if (iRes != 0)
	{
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
		goto error_irq_uninstall;
#else
		goto error_system_deinit;
#endif
	}
#else
	drm_mode_config_init(dev);
#endif /* defined(SUPPORT_DRM_DC_MODULE) */

	goto exit;

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
#if defined(SUPPORT_DRM_DC_MODULE)
error_irq_uninstall:
	drm_irq_uninstall(dev);
#endif

error_system_data_release:
	SysReleaseSystemData(psDevPriv->hSysData);
#endif

error_system_deinit:
	PVRSRVSystemDeInit();

error_free_dev_priv:
	dev->dev_private = NULL;
	OSFreeMem(psDevPriv);

error_exit:
	wake_up_interruptible(&sWaitForInit);

	bInitFailed = IMG_TRUE;

exit:
	bInitComplete = IMG_TRUE;

	return iRes;
}

static int PVRSRVDRMUnload(struct drm_device *dev)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	IMG_HANDLE hSysData = psDevPriv->hSysData;
#endif

	PVR_TRACE(("PVRSRVDRMUnload"));

#if defined(SUPPORT_DRM_DC_MODULE)
	(void)PVRSRVDRMDisplayDeinit(dev);
#endif

	psDevPriv->dev_node = NULL;
	PVRSRVSystemDeInit();

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	psDevPriv->hSysData = NULL;

	SysReleaseSystemData(hSysData);

	drm_irq_uninstall(dev);
#endif

	return 0;
}

static int PVRSRVDRMOpen(struct drm_device *dev, struct drm_file *file)
{
	while (!bInitComplete)
	{
		DEFINE_WAIT(sWait);

		prepare_to_wait(&sWaitForInit, &sWait, TASK_INTERRUPTIBLE);

		if (!bInitComplete)
		{
			PVR_TRACE(("%s: Waiting for module initialisation to complete", __FUNCTION__));

			schedule();
		}

		finish_wait(&sWaitForInit, &sWait);

		if (signal_pending(current))
		{
			return -ERESTARTSYS;
		}
	}

	if (bInitFailed)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Module initialisation failed", __FUNCTION__));
		return -EINVAL;
	}

	return PVRSRVOpen(dev, file);
}

#if !defined(NO_HARDWARE)
#if defined(SUPPORT_DRM_DC_MODULE) || defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
static irqreturn_t PVRSRVDRMIRQHandler(DRM_IRQ_ARGS)
#else
static irqreturn_t PVRSRVDRMIRQHandler(int irq, void *arg)
#endif
{
	struct drm_device *dev = arg;
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;

	PVR_UNREFERENCED_PARAMETER(irq);

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	if (psDevPriv->hSysData)
	{
		if (SystemISRHandler(psDevPriv->hSysData))
		{
			return IRQ_HANDLED;
		}
	}
#else
	if (psDevPriv->irq_handler)
	{
		return psDevPriv->irq_handler(psDevPriv->display_priv);
	}	
#endif

	return IRQ_NONE;
}
#endif
#endif /* !defined(NO_HARDWARE) */

#if !defined(SUPPORT_KERNEL_SRVINIT)
static int PVRDRMUnprivCmd(struct drm_device *dev, void *arg, struct drm_file *file)
{
	int ret = 0;

	OSAcquireBridgeLock();

	if (arg == NULL)
	{
		ret = -EFAULT;
	}
	else
	{
		drm_pvr_unpriv_cmd *psCommand = (drm_pvr_unpriv_cmd *)arg;

		switch (psCommand->cmd)
		{
			case DRM_PVR_UNPRIV_CMD_INIT_SUCCESFUL:
				psCommand->result = PVRSRVGetInitServerState(PVRSRV_INIT_SERVER_SUCCESSFUL) ? 1 : 0;
				break;
			default:
				ret = -EFAULT;
		}
	}

	OSReleaseBridgeLock();

	return ret;
}
#endif	/* #if !defined(SUPPORT_KERNEL_SRVINIT) */

#if defined(PDUMP)

static int PVRSRVDRMDbgDrvIoctl(struct drm_device *dev, IMG_VOID *arg, struct drm_file *pFile)
{
#if defined(CONFIG_COMPAT)
	IOCTL_PACKAGE *pIP = (IOCTL_PACKAGE *) arg;
	/* check whether the compatibility mode is required for this call */
	if(pIP->ui32PtrSize != __SIZEOF_POINTER__)
	{
		return dbgdrv_ioctl_compat(dev, arg, pFile);
	}
#endif
	return dbgdrv_ioctl(dev, arg, pFile);
}

#endif /* defined(PDUMP) */

/*
 * The big kernel lock is taken for ioctls unless the DRM_UNLOCKED flag is set.
 * If you revise one of the driver specific ioctls, or add a new one, that has
 * DRM_LOCKED set then consider whether the gPVRSRVLock mutex needs to be taken.
 */
struct drm_ioctl_desc sPVRDRMIoctls[] =
{
	DRM_IOCTL_DEF_DRV(PVR_SRVKM_CMD, PVRSRV_BridgeDispatchKM, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
#if !defined(SUPPORT_KERNEL_SRVINIT)
	DRM_IOCTL_DEF_DRV(PVR_UNPRIV_CMD, PVRDRMUnprivCmd, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
#endif	/* #if !defined(SUPPORT_KERNEL_SRVINIT) */
#if defined(PDUMP)
	DRM_IOCTL_DEF_DRV(PVR_DBGDRV_CMD, PVRSRVDRMDbgDrvIoctl, DRM_AUTH | DRM_UNLOCKED),
#endif
	DRM_IOCTL_DEF_DRV(PVR_GEM_CREATE, PVRDRMGEMCreate, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_GEM_TO_IMG_HANDLE, PVRDRMGEMToIMGHandle, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_IMG_TO_GEM_HANDLE, PVRDRMIMGToGEMHandle, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PVR_GEM_SYNC_GET, PVRDRMGEMSyncGet, PVR_DRM_RENDER_ALLOW | DRM_UNLOCKED),
};

#if defined(CONFIG_COMPAT)
static drm_ioctl_compat_t *apfnPVRDRMCompatIoctls[] =
{
	[DRM_PVR_SRVKM_CMD] = PVRSRV_BridgeCompatDispatchKM,
};

static long PVRSRVDRMCompatIoctl(struct file *file,
				 unsigned int cmd,
				 unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *pfnBridge = NULL;

	if (nr < DRM_COMMAND_BASE)
	{
		return drm_compat_ioctl(file, cmd, arg);
	}

	if (nr < DRM_COMMAND_BASE + ARRAY_SIZE(apfnPVRDRMCompatIoctls))
	{
		pfnBridge = apfnPVRDRMCompatIoctls[nr - DRM_COMMAND_BASE];
	}

	if (pfnBridge)
	{
		return pfnBridge(file, cmd, arg);
	}

	return drm_ioctl(file, cmd, arg);
}
#endif /* defined(CONFIG_COMPAT) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
static const struct file_operations sPVRFileOps =
{
	.owner			= THIS_MODULE,
	.open			= drm_open,
	.release		= drm_release,
	/*
	 * FIXME:
	 * Wrap this in a function that checks enough data has been
	 * supplied with the ioctl (e.g. _IOCDIR(nr) != _IOC_NONE &&
	 * _IOC_SIZE(nr) == size).
	 */
	.unlocked_ioctl		= drm_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl		= PVRSRVDRMCompatIoctl,
#endif
	.mmap			= MMapPMR,
	.poll			= drm_poll,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)) && ! defined(CHROMIUMOS_WORKAROUNDS_KERNEL310)
	.fasync			= drm_fasync,
#endif
	.read			= drm_read,
};
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */

struct drm_driver sPVRDRMDriver =
{
	.driver_features =
#if defined(PVR_DRM_USE_PRIME)
				DRIVER_PRIME |
#endif
#if !defined(NO_HARDWARE)
				DRIVER_HAVE_IRQ |
#endif
				DRIVER_MODESET |
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0))
				DRIVER_RENDER |
#endif
				DRIVER_GEM,

	.dev_priv_size		= 0,
	.load			= PVRSRVDRMLoad,
	.unload			= PVRSRVDRMUnload,
	.open			= PVRSRVDRMOpen,
	.postclose		= PVRSRVRelease,
#if !defined(NO_HARDWARE)
#if defined(SUPPORT_DRM_DC_MODULE)
	.get_vblank_counter	= PVRSRVDRMDisplayGetVBlankCounter,
	.enable_vblank		= PVRSRVDRMDisplayEnableVBlank,
	.disable_vblank		= PVRSRVDRMDisplayDisableVBlank,
	.dumb_create		= PVRSRVGEMDumbCreate,
	.dumb_map_offset	= PVRSRVGEMDumbMapOffset,
	.dumb_destroy		= PVRSRVGEMDumbDestroy,
#endif
#if defined(SUPPORT_DRM_DC_MODULE) || defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	.irq_handler		= PVRSRVDRMIRQHandler,
#endif
#endif
	.gem_free_object	= PVRSRVGEMFreeObject,
#if defined(PVR_DRM_USE_PRIME)
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= PVRSRVPrimeExport,
	.gem_prime_import	= PVRSRVPrimeImport,
#endif
	.ioctls			= sPVRDRMIoctls,
	.num_ioctls		= ARRAY_SIZE(sPVRDRMIoctls),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	.fops			= &sPVRFileOps,
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */
	.fops =
	{
		.owner		= THIS_MODULE,
		.open		= drm_open,
		.release	= drm_release,
		.unlocked_ioctl	= drm_ioctl,
#if defined(CONFIG_COMPAT)
		.compat_ioctl	= PVRSRVDRMCompatIoctl,
#endif
		.mmap		= MMapPMR,
		.poll		= drm_poll,
		.fasync		= drm_fasync,
		.read		= drm_read,
	},
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)) */
	.name			= PVR_DRM_NAME,
	.desc			= PVR_DRM_DESC,
	.date			= PVR_DRM_DATE,
	.major			= PVRVERSION_MAJ,
	.minor			= PVRVERSION_MIN,
	.patchlevel		= PVRVERSION_BUILD,
};
#endif	/* defined(SUPPORT_DRM) */
