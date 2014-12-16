/*************************************************************************/ /*!
@File
@Title          RGX sync kick routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX sync kick routines
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "srvkm.h"
#include "pdump_km.h"
#include "pvr_debug.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "osfunc.h"
#include "rgxccb.h"

#include "sync_server.h"
#include "sync_internal.h"

#include "rgxsync.h"

PVRSRV_ERROR RGXKickSyncKM(PVRSRV_DEVICE_NODE        *psDeviceNode,
                           RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
						   RGXFWIF_DM                eDM,
						   IMG_CHAR                  *pszCommandName,
                           IMG_UINT32                ui32ClientFenceCount,
                           PRGXFWIF_UFO_ADDR         *pauiClientFenceUFOAddress,
                           IMG_UINT32                *paui32ClientFenceValue,
                           IMG_UINT32                ui32ClientUpdateCount,
                           PRGXFWIF_UFO_ADDR         *pauiClientUpdateUFOAddress,
                           IMG_UINT32                *paui32ClientUpdateValue,
                           IMG_UINT32                ui32ServerSyncPrims,
                           IMG_UINT32                *paui32ServerSyncFlags,
                           SERVER_SYNC_PRIMITIVE     **pasServerSyncs,
                           IMG_BOOL                  bPDumpContinuous)
{
	RGXFWIF_KCCB_CMD		sCmpKCCBCmd;
	RGX_CCB_CMD_HELPER_DATA	asCmdHelperData[1];
	IMG_BOOL				bKickRequired;
	PVRSRV_ERROR			eError;
	PVRSRV_ERROR			eError2;
	IMG_UINT32				i;

	/* Sanity check the server fences */
	for (i=0;i<ui32ServerSyncPrims;i++)
	{
		if (!(paui32ServerSyncFlags[i] & PVRSRV_CLIENT_SYNC_PRIM_OP_CHECK))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Server fence (on %s) must fence", __FUNCTION__, pszCommandName));
			return PVRSRV_ERROR_INVALID_SYNC_PRIM_OP;
		}
	}

	eError = RGXCmdHelperInitCmdCCB(FWCommonContextGetClientCCB(psServerCommonContext),
									  ui32ClientFenceCount,
									  pauiClientFenceUFOAddress,
									  paui32ClientFenceValue,
									  ui32ClientUpdateCount,
									  pauiClientUpdateUFOAddress,
									  paui32ClientUpdateValue,
									  ui32ServerSyncPrims,
									  paui32ServerSyncFlags,
									  pasServerSyncs,
									  0,         /* ui32CmdSize */
									  IMG_NULL,  /* pui8DMCmd */
									  IMG_NULL,  /* ppPreAddr */
									  IMG_NULL,  /* ppPostAddr */
									  IMG_NULL,  /* ppRMWUFOAddr */
									  RGXFWIF_CCB_CMD_TYPE_NULL,
									  bPDumpContinuous,
									  pszCommandName,
									  asCmdHelperData);
	if (eError != PVRSRV_OK)
	{
		goto fail_cmdinit;
	}

	eError = RGXCmdHelperAcquireCmdCCB(IMG_ARR_NUM_ELEMS(asCmdHelperData),
	                                   asCmdHelperData, &bKickRequired);
	if ((eError != PVRSRV_OK) && (!bKickRequired))
	{
		/*
			Only bail if no new data was submitted into the client CCB, we might
			have already submitted a padding packet which we should flush through
			the FW.
		*/
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create client CCB command", __FUNCTION__));
		goto fail_cmdaquire;
	}


	/*
		We should reserved space in the kernel CCB here and fill in the command
		directly.
		This is so if there isn't space in the kernel CCB we can return with
		retry back to services client before we take any operations
	*/

	/*
		We might only be kicking for flush out a padding packet so only submit
		the command if the create was successful
	*/
	if (eError == PVRSRV_OK)
	{
		/*
			All the required resources are ready at this point, we can't fail so
			take the required server sync operations and commit all the resources
		*/
		RGXCmdHelperReleaseCmdCCB(1, asCmdHelperData, pszCommandName, FWCommonContextGetFWAddress(psServerCommonContext).ui32Addr);
	}

	/* Construct the kernel compute CCB command. */
	sCmpKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	sCmpKCCBCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psServerCommonContext);
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(FWCommonContextGetClientCCB(psServerCommonContext));
	sCmpKCCBCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;

	/*
	 * Submit the compute command to the firmware.
	 */
	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError2 = RGXScheduleCommand(psDeviceNode->pvDevice,
									eDM,
									&sCmpKCCBCmd,
									sizeof(sCmpKCCBCmd),
									bPDumpContinuous);
		if (eError2 != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();
	
	if (eError2 != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s failed to schedule kernel CCB command. (0x%x)", __FUNCTION__, eError));
	}
	/*
	 * Now check eError (which may have returned an error from our earlier call
	 * to RGXCmdHelperAcquireCmdCCB) - we needed to process any flush command first
	 * so we check it now...
	 */
	if (eError != PVRSRV_OK )
	{
		goto fail_cmdaquire;
	}

	return PVRSRV_OK;

fail_cmdaquire:
fail_cmdinit:

	return eError;
}

/******************************************************************************
 End of file (rgxsync.c)
******************************************************************************/
