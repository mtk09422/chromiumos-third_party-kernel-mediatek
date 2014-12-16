/*************************************************************************/ /*!
@File
@Title          PVR Bridge Module (kernel side)
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Receives calls from the user portion of services and
                despatches them to functions in the kernel portion.
@License        Strictly Confidential.
*/ /**************************************************************************/
#include "img_defs.h"
#include "pvr_bridge.h"
#include "connection_server.h"
#include "syscommon.h"
#include "pvr_debug.h"
#include "pvr_debugfs.h"
#include "private_data.h"
#include "linkage.h"

#if defined(SUPPORT_DRM)
#include <drm/drmP.h>
#include "pvr_drm.h"
#endif /* defined(SUPPORT_DRM) */

/* RGX: */
#if defined(SUPPORT_RGX)
#include "rgx_bridge.h"
#endif

#include "srvcore.h"
#include "common_srvcore_bridge.h"
#include "cache_defines.h"

#if defined(MODULE_TEST)
/************************************************************************/
// additional includes for services testing
/************************************************************************/
#include "pvr_test_bridge.h"
#include "kern_test.h"
/************************************************************************/
// end of additional includes
/************************************************************************/
#endif

#if defined(DEBUG_BRIDGE_KM)
static struct dentry *gpsPVRDebugFSBridgeStatsEntry = NULL;
static struct seq_operations gsBridgeStatsReadOps;
#endif

PVRSRV_ERROR InitPDUMPCTRLBridge(void);
PVRSRV_ERROR DeinitPDUMPCTRLBridge(void);
#if defined(SUPPORT_DISPLAY_CLASS)
PVRSRV_ERROR InitDCBridge(void);
PVRSRV_ERROR DeinitDCBridge(void);
#endif
PVRSRV_ERROR InitMMBridge(void);
PVRSRV_ERROR DeinitMMBridge(void);
PVRSRV_ERROR InitCMMBridge(void);
PVRSRV_ERROR DeinitCMMBridge(void);
PVRSRV_ERROR InitPDUMPMMBridge(void);
PVRSRV_ERROR DeinitPDUMPMMBridge(void);
PVRSRV_ERROR InitPDUMPBridge(void);
PVRSRV_ERROR DeinitPDUMPBridge(void);
PVRSRV_ERROR InitSRVCOREBridge(void);
PVRSRV_ERROR DeinitSRVCOREBridge(void);
PVRSRV_ERROR InitSYNCBridge(void);
PVRSRV_ERROR DeinitSYNCBridge(void);
#if defined(SUPPORT_INSECURE_EXPORT)
PVRSRV_ERROR InitSYNCEXPORTBridge(void);
PVRSRV_ERROR DeinitSYNCEXPORTBridge(void);
#endif
#if defined(SUPPORT_SECURE_EXPORT)
PVRSRV_ERROR InitSYNCSEXPORTBridge(void);
PVRSRV_ERROR DeinitSYNCSEXPORTBridge(void);
#endif
#if defined (SUPPORT_RGX)
PVRSRV_ERROR InitRGXINITBridge(void);
PVRSRV_ERROR DeinitRGXINITBridge(void);
PVRSRV_ERROR InitRGXTA3DBridge(void);
PVRSRV_ERROR DeinitRGXTA3DBridge(void);
PVRSRV_ERROR InitRGXTQBridge(void);
PVRSRV_ERROR DeinitRGXTQBridge(void);
PVRSRV_ERROR InitRGXCMPBridge(void);
PVRSRV_ERROR DeinitRGXCMPBridge(void);
PVRSRV_ERROR InitBREAKPOINTBridge(void);
PVRSRV_ERROR DeinitBREAKPOINTBridge(void);
PVRSRV_ERROR InitDEBUGMISCBridge(void);
PVRSRV_ERROR DeinitDEBUGMISCBridge(void);
PVRSRV_ERROR InitRGXPDUMPBridge(void);
PVRSRV_ERROR DeinitRGXPDUMPBridge(void);
PVRSRV_ERROR InitRGXHWPERFBridge(void);
PVRSRV_ERROR DeinitRGXHWPERFBridge(void);
#if defined(RGX_FEATURE_RAY_TRACING)
PVRSRV_ERROR InitRGXRAYBridge(void);
PVRSRV_ERROR DeinitRGXRAYBridge(void);
#endif /* RGX_FEATURE_RAY_TRACING */
PVRSRV_ERROR InitREGCONFIGBridge(void);
PVRSRV_ERROR DeinitREGCONFIGBridge(void);
PVRSRV_ERROR InitTIMERQUERYBridge(void);
PVRSRV_ERROR DeinitTIMERQUERYBridge(void);
#endif /* SUPPORT_RGX */
#if (CACHEFLUSH_TYPE == CACHEFLUSH_GENERIC)
PVRSRV_ERROR InitCACHEGENERICBridge(void);
PVRSRV_ERROR DeinitCACHEGENERICBridge(void);
#endif
#if defined(SUPPORT_SECURE_EXPORT)
PVRSRV_ERROR InitSMMBridge(void);
PVRSRV_ERROR DeinitSMMBridge(void);
#endif
PVRSRV_ERROR InitPVRTLBridge(void);
PVRSRV_ERROR DeinitPVRTLBridge(void);
#if defined(PVR_RI_DEBUG)
PVRSRV_ERROR InitRIBridge(void);
PVRSRV_ERROR DeinitRIBridge(void);
#endif
#if defined(SUPPORT_ION)
PVRSRV_ERROR InitDMABUFBridge(void);
PVRSRV_ERROR DeinitDMABUFBridge(void);
#endif
#if defined(SUPPORT_VALIDATION)
PVRSRV_ERROR InitVALIDATIONBridge(void);
#endif

/* These and their friends above will go when full bridge gen comes in */
PVRSRV_ERROR
LinuxBridgeInit(void);
PVRSRV_ERROR
LinuxBridgeDeInit(void);

PVRSRV_ERROR
LinuxBridgeInit(void)
{
	PVRSRV_ERROR eError;
#if defined(DEBUG_BRIDGE_KM)
	IMG_INT iResult;

	iResult = PVRDebugFSCreateEntry("bridge_stats",
					NULL,
					&gsBridgeStatsReadOps,
					NULL,
					&g_BridgeDispatchTable[0],
					&gpsPVRDebugFSBridgeStatsEntry);
	if (iResult != 0)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
#endif

	eError = InitSRVCOREBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitSYNCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_INSECURE_EXPORT)
	eError = InitSYNCEXPORTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif
#if defined(SUPPORT_SECURE_EXPORT)
	eError = InitSYNCSEXPORTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = InitPDUMPCTRLBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = InitMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = InitCMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = InitPDUMPMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = InitPDUMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_ION)
	eError = InitDMABUFBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_DISPLAY_CLASS)
	eError = InitDCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if (CACHEFLUSH_TYPE == CACHEFLUSH_GENERIC)
	eError = InitCACHEGENERICBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_SECURE_EXPORT)
	eError = InitSMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = InitPVRTLBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	#if defined(PVR_RI_DEBUG)
	eError = InitRIBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	#endif

#if defined(SUPPORT_VALIDATION)
	eError = InitVALIDATIONBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	#if defined (SUPPORT_RGX)
	eError = InitRGXTQBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitRGXCMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitRGXINITBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitRGXTA3DBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitBREAKPOINTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitDEBUGMISCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	
	eError = InitRGXPDUMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitRGXHWPERFBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(RGX_FEATURE_RAY_TRACING)
	eError = InitRGXRAYBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif /* RGX_FEATURE_RAY_TRACING */

	eError = InitREGCONFIGBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = InitTIMERQUERYBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#endif /* SUPPORT_RGX */

	return eError;
}

PVRSRV_ERROR
LinuxBridgeDeInit(void)
{
	PVRSRV_ERROR eError;
#if defined(DEBUG_BRIDGE_KM)
	if (gpsPVRDebugFSBridgeStatsEntry != NULL)
	{
		PVRDebugFSRemoveEntry(gpsPVRDebugFSBridgeStatsEntry);
		gpsPVRDebugFSBridgeStatsEntry = NULL;
	}
#endif

	eError = DeinitSRVCOREBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitSYNCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_INSECURE_EXPORT)
	eError = DeinitSYNCEXPORTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif
#if defined(SUPPORT_SECURE_EXPORT)
	eError = DeinitSYNCSEXPORTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = DeinitPDUMPCTRLBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = DeinitMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = DeinitCMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = DeinitPDUMPMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = DeinitPDUMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_ION)
	eError = DeinitDMABUFBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_DISPLAY_CLASS)
	eError = DeinitDCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if (CACHEFLUSH_TYPE == CACHEFLUSH_GENERIC)
	eError = DeinitCACHEGENERICBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_SECURE_EXPORT)
	eError = DeinitSMMBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = DeinitPVRTLBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	#if defined(PVR_RI_DEBUG)
	eError = DeinitRIBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	#endif

	#if defined (SUPPORT_RGX)
	eError = DeinitRGXTQBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitRGXCMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitRGXINITBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitRGXTA3DBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitBREAKPOINTBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitDEBUGMISCBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	
	eError = DeinitRGXPDUMPBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitRGXHWPERFBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(RGX_FEATURE_RAY_TRACING)
	eError = DeinitRGXRAYBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif /* RGX_FEATURE_RAY_TRACING */

	eError = DeinitREGCONFIGBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = DeinitTIMERQUERYBridge();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#endif /* SUPPORT_RGX */

	return eError;
}

#if defined(DEBUG_BRIDGE_KM)
static void *BridgeStatsSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psDispatchTable = (PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)psSeqFile->private;

	OSAcquireBridgeLock();

	if (psDispatchTable == NULL || (*puiPosition) > BRIDGE_DISPATCH_TABLE_ENTRY_COUNT)
	{
		return NULL;
	}

	if ((*puiPosition) == 0) 
	{
		return SEQ_START_TOKEN;
	}

	return &(psDispatchTable[(*puiPosition) - 1]);
}

static void BridgeStatsSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);

	OSReleaseBridgeLock();
}

static void *BridgeStatsSeqNext(struct seq_file *psSeqFile,
			       void *pvData,
			       loff_t *puiPosition)
{
	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psDispatchTable = (PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)psSeqFile->private;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	if ((*puiPosition) > BRIDGE_DISPATCH_TABLE_ENTRY_COUNT)
	{
		return NULL;
	}

	return &(psDispatchTable[(*puiPosition) - 1]);
}

static int BridgeStatsSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData == SEQ_START_TOKEN)
	{
		seq_printf(psSeqFile,
			   "Total ioctl call count = %u\n"
			   "Total number of bytes copied via copy_from_user = %u\n"
			   "Total number of bytes copied via copy_to_user = %u\n"
			   "Total number of bytes copied via copy_*_user = %u\n\n"
			   "%-60s | %-48s | %10s | %20s | %10s\n",
			   g_BridgeGlobalStats.ui32IOCTLCount,
			   g_BridgeGlobalStats.ui32TotalCopyFromUserBytes,
			   g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
			   g_BridgeGlobalStats.ui32TotalCopyFromUserBytes + g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
			   "Bridge Name",
			   "Wrapper Function",
			   "Call Count",
			   "copy_from_user Bytes",
			   "copy_to_user Bytes");
	}
	else if (pvData != NULL)
	{
		PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psEntry = (	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)pvData;

		seq_printf(psSeqFile,
			   "%-60s   %-48s   %-10u   %-20u   %-10u\n",
			   psEntry->pszIOCName,
			   psEntry->pszFunctionName,
			   psEntry->ui32CallCount,
			   psEntry->ui32CopyFromUserTotalBytes,
			   psEntry->ui32CopyToUserTotalBytes);
	}

	return 0;
}

static struct seq_operations gsBridgeStatsReadOps =
{
	.start = BridgeStatsSeqStart,
	.stop = BridgeStatsSeqStop,
	.next = BridgeStatsSeqNext,
	.show = BridgeStatsSeqShow,
};
#endif /* defined(DEBUG_BRIDGE_KM) */


#if defined(SUPPORT_DRM)
int
PVRSRV_BridgeDispatchKM(struct drm_device unref__ *dev, void *arg, struct drm_file *pDRMFile)
#else
long
PVRSRV_BridgeDispatchKM(struct file *pFile, unsigned int unref__ ioctlCmd, unsigned long arg)
#endif
{
#if defined(SUPPORT_DRM)
	struct file *pFile = PVR_FILE_FROM_DRM_FILE(pDRMFile);
#else
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageUM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVRSRV_BRIDGE_PACKAGE sBridgePackageKM;
#endif
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM;
	CONNECTION_DATA *psConnection = LinuxConnectionFromFile(pFile);

#if defined(SUPPORT_DRM)
	psBridgePackageKM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVR_ASSERT(psBridgePackageKM != IMG_NULL);
#else

	psBridgePackageKM = &sBridgePackageKM;

	if (!OSAccessOK(PVR_VERIFY_WRITE,
				   psBridgePackageUM,
				   sizeof(PVRSRV_BRIDGE_PACKAGE)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Received invalid pointer to function arguments",
				 __FUNCTION__));

		return -EFAULT;
	}
	
	/* FIXME - Currently the CopyFromUserWrapper which collects stats about
	 * how much data is shifted to/from userspace isn't available to us
	 * here. */
	if (OSCopyFromUser(IMG_NULL,
					  psBridgePackageKM,
					  psBridgePackageUM,
					  sizeof(PVRSRV_BRIDGE_PACKAGE))
	  != PVRSRV_OK)
	{
		return -EFAULT;
	}
#endif

#if defined(DEBUG_BRIDGE_CALLS)
	{
		IMG_UINT32 mangledID;
		mangledID = psBridgePackageKM->ui32BridgeID;

		psBridgePackageKM->ui32BridgeID = PVRSRV_GET_BRIDGE_ID(psBridgePackageKM->ui32BridgeID);

		PVR_DPF((PVR_DBG_WARNING, "Bridge ID (x%8x) %8u (mangled: x%8x) ", psBridgePackageKM->ui32BridgeID, psBridgePackageKM->ui32BridgeID, mangledID));
	}
#else
		psBridgePackageKM->ui32BridgeID = PVRSRV_GET_BRIDGE_ID(psBridgePackageKM->ui32BridgeID);
#endif

	return BridgedDispatchKM(psConnection, psBridgePackageKM);
}


#if defined(CONFIG_COMPAT)
#if defined(SUPPORT_DRM)
int
#else
long
#endif
PVRSRV_BridgeCompatDispatchKM(struct file *pFile,
			      unsigned int unref__ ioctlCmd,
			      unsigned long arg)
{
	struct bridge_package_from_32
	{
		IMG_UINT32				bridge_id;			/*!< ioctl/drvesc index */
		IMG_UINT32				size;				/*!< size of structure */
		IMG_UINT32				addr_param_in;		/*!< input data buffer */ 
		IMG_UINT32				in_buffer_size;		/*!< size of input data buffer */
		IMG_UINT32				addr_param_out;		/*!< output data buffer */
		IMG_UINT32				out_buffer_size;	/*!< size of output data buffer */
	};

	PVRSRV_BRIDGE_PACKAGE params_for_64;
	struct bridge_package_from_32 params;
 	struct bridge_package_from_32 * const params_addr = &params;
	CONNECTION_DATA *psConnection = LinuxConnectionFromFile(pFile);

	/* make sure there is no padding inserted by compiler */
	PVR_ASSERT(sizeof(struct bridge_package_from_32) == 6 * sizeof(IMG_UINT32));

	if(!OSAccessOK(PVR_VERIFY_READ, (void *) arg,
				   sizeof(struct bridge_package_from_32)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Received invalid pointer to function arguments",
				 __FUNCTION__));

		return -EFAULT;
	}
	
	if(OSCopyFromUser(NULL, params_addr, (void*) arg,
					  sizeof(struct bridge_package_from_32))
	   != PVRSRV_OK)
	{
		return -EFAULT;
	}

	PVR_ASSERT(params_addr->size == sizeof(struct bridge_package_from_32));

	params_addr->bridge_id = PVRSRV_GET_BRIDGE_ID(params_addr->bridge_id);

#if defined(DEBUG_BRIDGE_KM)
	PVR_DPF((PVR_DBG_MESSAGE, "ioctl %s -> func %s",
		g_BridgeDispatchTable[params_addr->bridge_id].pszIOCName,
		g_BridgeDispatchTable[params_addr->bridge_id].pszFunctionName));
#endif

	params_for_64.ui32BridgeID = params_addr->bridge_id;
	params_for_64.ui32Size = sizeof(params_for_64);
	params_for_64.pvParamIn = (void*) ((size_t) params_addr->addr_param_in);
	params_for_64.pvParamOut = (void*) ((size_t) params_addr->addr_param_out);
	params_for_64.ui32InBufferSize = params_addr->in_buffer_size;
	params_for_64.ui32OutBufferSize = params_addr->out_buffer_size;

	return BridgedDispatchKM(psConnection, &params_for_64);
}
#endif /* defined(CONFIG_COMPAT) */
