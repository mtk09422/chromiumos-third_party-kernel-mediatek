/*************************************************************************/ /*!
@File
@Title          Server bridge for pdumpctrl
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for pdumpctrl
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "pdump_km.h"


#include "common_pdumpctrl_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#include "rgx_bridge.h"
#include "srvcore.h"
#include "handle.h"

#if defined (SUPPORT_AUTH)
#include "osauth.h"
#endif

#include <linux/slab.h>

#include "lock.h"



/* ***************************************************************************
 * Server-side bridge entry points
 */
 
static IMG_INT
PVRSRVBridgePVRSRVPDumpIsCapturing(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPISCAPTURING *psPVRSRVPDumpIsCapturingIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISCAPTURING *psPVRSRVPDumpIsCapturingOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISCAPTURING);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpIsCapturingIN);






	psPVRSRVPDumpIsCapturingOUT->eError =
		PDumpIsCaptureFrameKM(
					&psPVRSRVPDumpIsCapturingOUT->bIsCapturing);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpSetFrame(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME *psPVRSRVPDumpSetFrameIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME *psPVRSRVPDumpSetFrameOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETFRAME);







	psPVRSRVPDumpSetFrameOUT->eError =
		PDumpSetFrameKM(psConnection,
					psPVRSRVPDumpSetFrameIN->ui32Frame);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpGetFrame(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPGETFRAME *psPVRSRVPDumpGetFrameIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPGETFRAME *psPVRSRVPDumpGetFrameOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPGETFRAME);

	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpGetFrameIN);






	psPVRSRVPDumpGetFrameOUT->eError =
		PDumpGetFrameKM(psConnection,
					&psPVRSRVPDumpGetFrameOUT->ui32Frame);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpSetDefaultCaptureParams(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS *psPVRSRVPDumpSetDefaultCaptureParamsIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS *psPVRSRVPDumpSetDefaultCaptureParamsOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS);

	PVR_UNREFERENCED_PARAMETER(psConnection);






	psPVRSRVPDumpSetDefaultCaptureParamsOUT->eError =
		PDumpSetDefaultCaptureParamsKM(
					psPVRSRVPDumpSetDefaultCaptureParamsIN->ui32Mode,
					psPVRSRVPDumpSetDefaultCaptureParamsIN->ui32Start,
					psPVRSRVPDumpSetDefaultCaptureParamsIN->ui32End,
					psPVRSRVPDumpSetDefaultCaptureParamsIN->ui32Interval,
					psPVRSRVPDumpSetDefaultCaptureParamsIN->ui32MaxParamFileSize);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpIsLastCaptureFrame(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPISLASTCAPTUREFRAME *psPVRSRVPDumpIsLastCaptureFrameIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISLASTCAPTUREFRAME *psPVRSRVPDumpIsLastCaptureFrameOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISLASTCAPTUREFRAME);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpIsLastCaptureFrameIN);






	psPVRSRVPDumpIsLastCaptureFrameOUT->eError =
		PDumpIsLastCaptureFrameKM(
					);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpStartInitPhase(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTARTINITPHASE *psPVRSRVPDumpStartInitPhaseIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTARTINITPHASE *psPVRSRVPDumpStartInitPhaseOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTARTINITPHASE);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpStartInitPhaseIN);






	psPVRSRVPDumpStartInitPhaseOUT->eError =
		PDumpStartInitPhaseKM(
					);





	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpStopInitPhase(IMG_UINT32 ui32BridgeID,
					  PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTOPINITPHASE *psPVRSRVPDumpStopInitPhaseIN,
					  PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTOPINITPHASE *psPVRSRVPDumpStopInitPhaseOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTOPINITPHASE);

	PVR_UNREFERENCED_PARAMETER(psConnection);






	psPVRSRVPDumpStopInitPhaseOUT->eError =
		PDumpStopInitPhaseKM(
					psPVRSRVPDumpStopInitPhaseIN->eModuleID);





	return 0;
}



/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */

static POS_LOCK pPDUMPCTRLBridgeLock;
static IMG_BYTE pbyPDUMPCTRLBridgeBuffer[20 +  8];

PVRSRV_ERROR InitPDUMPCTRLBridge(IMG_VOID);
PVRSRV_ERROR DeinitPDUMPCTRLBridge(IMG_VOID);

/*
 * Register all PDUMPCTRL functions with services
 */
PVRSRV_ERROR InitPDUMPCTRLBridge(IMG_VOID)
{
	PVR_LOGR_IF_ERROR(OSLockCreate(&pPDUMPCTRLBridgeLock, LOCK_TYPE_PASSIVE), "OSLockCreate");

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISCAPTURING, PVRSRVBridgePVRSRVPDumpIsCapturing,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETFRAME, PVRSRVBridgePVRSRVPDumpSetFrame,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPGETFRAME, PVRSRVBridgePVRSRVPDumpGetFrame,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS, PVRSRVBridgePVRSRVPDumpSetDefaultCaptureParams,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISLASTCAPTUREFRAME, PVRSRVBridgePVRSRVPDumpIsLastCaptureFrame,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTARTINITPHASE, PVRSRVBridgePVRSRVPDumpStartInitPhase,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);

	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTOPINITPHASE, PVRSRVBridgePVRSRVPDumpStopInitPhase,
					pPDUMPCTRLBridgeLock, pbyPDUMPCTRLBridgeBuffer,
					20,  8);


	return PVRSRV_OK;
}

/*
 * Unregister all pdumpctrl functions with services
 */
PVRSRV_ERROR DeinitPDUMPCTRLBridge(IMG_VOID)
{
	PVR_LOGR_IF_ERROR(OSLockDestroy(pPDUMPCTRLBridgeLock), "OSLockDestroy");
	return PVRSRV_OK;
}

