/*************************************************************************/ /*!
@File
@Title          MT8173 module setup
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <drm/drmP.h>
#include "pvr_debug.h"
#include "srvkm.h"
#include "pvrmodule.h"
#include "linkage.h"
#include "sysinfo.h"
#include "module_common.h"
#include "syscommon.h"
#include "pvr_drm.h"

#if defined(SUPPORT_SHARED_SLC)
#include "rgxapi_km.h"
#endif

/*
 * DRVNAME is the name we use to register our driver.
 * DEVNAME is the name we use to register actual device nodes.
 */
#define	DRVNAME		PVR_LDM_DRIVER_REGISTRATION_NAME
#define DEVNAME		PVRSRV_MODNAME

/*
 * This is all module configuration stuff required by the linux kernel.
 */
MODULE_SUPPORTED_DEVICE(DEVNAME);

#if defined(SUPPORT_SHARED_SLC)
EXPORT_SYMBOL(RGXInitSLC);
#endif

extern int MTKMFGGetClocks(struct platform_device* pDevice);
static int PVRSRVDriverRemove(struct platform_device *device);
static int PVRSRVDriverProbe(struct platform_device *device);

static struct dev_pm_ops powervr_dev_pm_ops = {
	.suspend	= PVRSRVDriverSuspend,
	.resume		= PVRSRVDriverResume,
};

static const struct of_device_id mt_powervr_of_match[] = {
#if defined(CONFIG_POWERVR_ROGUE_MT8135)
	{ .compatible = "mediatek,mt8135-gpu", },
#else
	{ .compatible = "mediatek,mt8173-gpu", },
#endif
	{},
};

MODULE_DEVICE_TABLE(of,mt_powervr_of_match);

static struct platform_driver powervr_driver = {
	.driver = {
		.name	= DRVNAME,
		.of_match_table = of_match_ptr(mt_powervr_of_match),
		.pm	= &powervr_dev_pm_ops,
	},
	.probe		= PVRSRVDriverProbe,
	.remove		= PVRSRVDriverRemove,
	.shutdown	= PVRSRVDriverShutdown,
};

#if defined(MODULE)
static struct platform_device_info powervr_device_info =
{
	.name			= DEVNAME,
	.id			= -1,
	.dma_mask		= DMA_BIT_MASK(32),
};
#endif	/* defined(MODULE) */

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
int PVRSRVSystemInit(struct drm_device *pDrmDevice)
{
	struct platform_device *pDevice = pDrmDevice->platformdev;

	PVR_TRACE(("PVRSRVSystemInit (pDevice=%p)", pDevice));

	/* PVRSRVInit is only designed to be called once */
	if (bCalledSysInit == IMG_FALSE)
	{
		gpsPVRLDMDev = pDevice;
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

 @input pDevice - the device for which a probe is requested
 @Return nothing.

*****************************************************************************/
void PVRSRVSystemDeInit(struct platform_device *pDevice)
{
	PVR_TRACE(("PVRSRVSystemDeInit"));

	PVRSRVDeInit(pDevice);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0))
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
static int PVRSRVDriverProbe(struct platform_device *pDevice)
{
	int result = 0;
	PVR_TRACE(("PVRSRVDriverProbe (pDevice=%p)", pDevice));

	if (OSStringCompare(pDevice->name,DEVNAME) != 0)
	{
		result = MTKMFGGetClocks(pDevice);
	}

	result = drm_platform_init(&sPVRDRMDriver, pDevice);
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
static int PVRSRVDriverRemove(struct platform_device *pDevice)
{
	PVR_TRACE(("PVRSRVDriverRemove (pDevice=%p)", pDevice));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
	drm_platform_exit(&sPVRDRMDriver, pDevice);
#else
	drm_put_dev(platform_get_drvdata(pDevice));
#endif
	return 0;
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
int PVRSRVOpen(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
{
	int err;

	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);

	if (!try_module_get(THIS_MODULE))
	{
		PVR_DPF((PVR_DBG_ERROR, "Failed to get module"));
		return -ENOENT;
	}

	if ((err = PVRSRVCommonOpen(pFile)) != 0)
	{
		module_put(THIS_MODULE);
	}

	return err;
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
void PVRSRVRelease(struct drm_device unref__ *dev, struct drm_file *pDRMFile)
{
	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);

	PVRSRVCommonRelease(pFile);

	module_put(THIS_MODULE);
}

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
static int __init PVRCore_Init(void)
{
	int error = 0;

	PVR_TRACE(("PVRCore_Init"));

#if defined(PDUMP)
	error = dbgdrv_init();
	if (error != 0)
	{
		return error;
	}
#endif

	if ((error = PVRSRVCommonPrepare()) != 0)
	{
		return error;
	}

	error = platform_driver_register(&powervr_driver);
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register platform driver (%d)", error));
		return error;
	}

#if defined(MODULE)
	gpsPVRLDMDev = platform_device_register_full(&powervr_device_info);
	error = IS_ERR(gpsPVRLDMDev) ? PTR_ERR(gpsPVRLDMDev) : 0;
	if (error != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRCore_Init: unable to register platform device (%d)", error));
		return error;
	}
#endif /* defined(MODULE) */

	/* Check that the driver probe function was called */
	if (!bDriverProbeSucceeded)
	{
		PVR_TRACE(("PVRCore_Init: PVRSRVDriverProbe has not been called or did not succeed - check that hardware is detected"));
		return error;
	}

	return PVRSRVCommonInit();
}


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
static void __exit PVRCore_Cleanup(void)
{
	PVR_TRACE(("PVRCore_Cleanup"));

	PVRSRVCommonDeinit();

#if defined(MODULE)
	PVR_ASSERT(gpsPVRLDMDev != NULL);
	platform_device_unregister(gpsPVRLDMDev);
#endif /* defined(MODULE) */
	platform_driver_unregister(&powervr_driver);

	PVRSRVCommonCleanup();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
	PVR_TRACE(("PVRCore_Cleanup: unloading"));
}

/*
 * These macro calls define the initialisation and removal functions of the
 * driver.  Although they are prefixed `module_', they apply when compiling
 * statically as well; in both cases they define the function the kernel will
 * run to start/stop the driver.
*/
module_init(PVRCore_Init);
module_exit(PVRCore_Cleanup);
