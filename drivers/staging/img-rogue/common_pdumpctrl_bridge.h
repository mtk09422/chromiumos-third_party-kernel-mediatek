/*************************************************************************/ /*!
@File
@Title          Common bridge header for pdumpctrl
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Declares common defines and structures that are used by both
                the client and sever side of the bridge for pdumpctrl
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef COMMON_PDUMPCTRL_BRIDGE_H
#define COMMON_PDUMPCTRL_BRIDGE_H

#include "services.h"


#include "pvr_bridge_io.h"

#define PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST			(PVRSRV_BRIDGE_PDUMPCTRL_START)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISCAPTURING			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+0)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETFRAME			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+1)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPGETFRAME			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+2)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+3)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPISLASTCAPTUREFRAME			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+4)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTARTINITPHASE			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+5)
#define PVRSRV_BRIDGE_PDUMPCTRL_PVRSRVPDUMPSTOPINITPHASE			PVRSRV_IOWR(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+6)
#define PVRSRV_BRIDGE_PDUMPCTRL_CMD_LAST			(PVRSRV_BRIDGE_PDUMPCTRL_CMD_FIRST+6)


/*******************************************
            PVRSRVPDumpIsCapturing          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpIsCapturing */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPISCAPTURING_TAG
{
	 IMG_UINT32 ui32EmptyStructPlaceholder;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPISCAPTURING;


/* Bridge out structure for PVRSRVPDumpIsCapturing */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISCAPTURING_TAG
{
	IMG_BOOL bIsCapturing;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISCAPTURING;

/*******************************************
            PVRSRVPDumpSetFrame          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpSetFrame */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME_TAG
{
	IMG_UINT32 ui32Frame;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME;


/* Bridge out structure for PVRSRVPDumpSetFrame */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME;

/*******************************************
            PVRSRVPDumpGetFrame          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpGetFrame */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPGETFRAME_TAG
{
	 IMG_UINT32 ui32EmptyStructPlaceholder;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPGETFRAME;


/* Bridge out structure for PVRSRVPDumpGetFrame */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPGETFRAME_TAG
{
	IMG_UINT32 ui32Frame;
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPGETFRAME;

/*******************************************
            PVRSRVPDumpSetDefaultCaptureParams          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpSetDefaultCaptureParams */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS_TAG
{
	IMG_UINT32 ui32Mode;
	IMG_UINT32 ui32Start;
	IMG_UINT32 ui32End;
	IMG_UINT32 ui32Interval;
	IMG_UINT32 ui32MaxParamFileSize;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS;


/* Bridge out structure for PVRSRVPDumpSetDefaultCaptureParams */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETDEFAULTCAPTUREPARAMS;

/*******************************************
            PVRSRVPDumpIsLastCaptureFrame          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpIsLastCaptureFrame */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPISLASTCAPTUREFRAME_TAG
{
	 IMG_UINT32 ui32EmptyStructPlaceholder;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPISLASTCAPTUREFRAME;


/* Bridge out structure for PVRSRVPDumpIsLastCaptureFrame */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISLASTCAPTUREFRAME_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISLASTCAPTUREFRAME;

/*******************************************
            PVRSRVPDumpStartInitPhase          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpStartInitPhase */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTARTINITPHASE_TAG
{
	 IMG_UINT32 ui32EmptyStructPlaceholder;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTARTINITPHASE;


/* Bridge out structure for PVRSRVPDumpStartInitPhase */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTARTINITPHASE_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTARTINITPHASE;

/*******************************************
            PVRSRVPDumpStopInitPhase          
 *******************************************/

/* Bridge in structure for PVRSRVPDumpStopInitPhase */
typedef struct PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTOPINITPHASE_TAG
{
	IMG_MODULE_ID eModuleID;
} __attribute__((packed)) PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTOPINITPHASE;


/* Bridge out structure for PVRSRVPDumpStopInitPhase */
typedef struct PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTOPINITPHASE_TAG
{
	PVRSRV_ERROR eError;
} __attribute__((packed)) PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTOPINITPHASE;

#endif /* COMMON_PDUMPCTRL_BRIDGE_H */
